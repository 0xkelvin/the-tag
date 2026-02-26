import 'dart:async';
import 'dart:typed_data';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'image_converter.dart';

/// UUIDs matching firmware ble_image_service.h
final Guid imageServiceUuid =
    Guid('12345678-1234-5678-1234-56789abcdef0');
final Guid imageDataCharUuid =
    Guid('12345678-1234-5678-1234-56789abcdef1');
final Guid imageCtrlCharUuid =
    Guid('12345678-1234-5678-1234-56789abcdef2');

/// Control commands
const int cmdStart = 0x01;
const int cmdCommit = 0x02;
const int cmdCancel = 0x03;

/// Status codes from firmware
const int statusReady = 0x00;
const int statusProgress = 0x01;
const int statusDisplaying = 0x10;
const int statusDone = 0x11;
const int statusError = 0xFF;

enum TransferState {
  idle,
  connecting,
  connected,
  sending,
  displaying,
  done,
  error,
}

class BleImageTransfer {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _ctrlChar;
  StreamSubscription? _ctrlNotifySub;
  StreamSubscription? _connectionSub;

  final _stateController = StreamController<TransferState>.broadcast();
  final _progressController = StreamController<double>.broadcast();
  final _logController = StreamController<String>.broadcast();

  Stream<TransferState> get stateStream => _stateController.stream;
  Stream<double> get progressStream => _progressController.stream;
  Stream<String> get logStream => _logController.stream;

  TransferState _state = TransferState.idle;
  TransferState get state => _state;

  BluetoothDevice? get connectedDevice => _device;

  void _setState(TransferState s) {
    _state = s;
    _stateController.add(s);
  }

  void _log(String msg) {
    _logController.add(msg);
  }

  Future<List<ScanResult>> scan({Duration timeout = const Duration(seconds: 4)}) async {
    _log('Scanning for devices...');
    final results = <ScanResult>[];

    await FlutterBluePlus.startScan(
      withServices: [imageServiceUuid],
      timeout: timeout,
    );

    await for (final r in FlutterBluePlus.onScanResults) {
      for (final sr in r) {
        if (!results.any((e) => e.device.remoteId == sr.device.remoteId)) {
          results.add(sr);
        }
      }
      if (!FlutterBluePlus.isScanningNow) break;
    }

    _log('Found ${results.length} device(s)');
    return results;
  }

  Future<void> connect(BluetoothDevice device) async {
    _setState(TransferState.connecting);
    _log('Connecting to ${device.platformName}...');

    try {
      await device.connect(
        license: License.free,
        timeout: const Duration(seconds: 10),
      );

      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _log('Device disconnected');
          _cleanup();
          _setState(TransferState.idle);
        }
      });

      _log('Connected. Discovering services...');
      final services = await device.discoverServices();

      BluetoothService? imgService;
      for (final s in services) {
        if (s.serviceUuid == imageServiceUuid) {
          imgService = s;
          break;
        }
      }

      if (imgService == null) {
        throw Exception('Image service not found on device');
      }

      for (final c in imgService.characteristics) {
        if (c.characteristicUuid == imageDataCharUuid) {
          _dataChar = c;
        } else if (c.characteristicUuid == imageCtrlCharUuid) {
          _ctrlChar = c;
        }
      }

      if (_dataChar == null || _ctrlChar == null) {
        throw Exception('Required characteristics not found');
      }

      // Request higher MTU for faster transfer
      await device.requestMtu(247);
      _log('MTU negotiated');

      // Subscribe to ctrl notifications
      await _ctrlChar!.setNotifyValue(true);
      _ctrlNotifySub = _ctrlChar!.onValueReceived.listen(_onCtrlNotify);

      _device = device;
      _setState(TransferState.connected);
      _log('Ready for image transfer');
    } catch (e) {
      _log('Connection failed: $e');
      _setState(TransferState.error);
      rethrow;
    }
  }

  void _onCtrlNotify(List<int> value) {
    if (value.isEmpty) return;

    switch (value[0]) {
      case statusReady:
        _log('Firmware ready for data');
        break;
      case statusProgress:
        if (value.length >= 3) {
          final received = value[1] | (value[2] << 8);
          final progress = received / bufferSize;
          _progressController.add(progress);
          _log('Progress: $received / $bufferSize bytes');
        }
        break;
      case statusDisplaying:
        _log('Firmware displaying image...');
        _setState(TransferState.displaying);
        break;
      case statusDone:
        _log('Display refresh complete!');
        _setState(TransferState.done);
        break;
      case statusError:
        final errCode = value.length > 1 ? value[1] : 0;
        _log('Firmware error: 0x${errCode.toRadixString(16)}');
        _setState(TransferState.error);
        break;
    }
  }

  Future<void> sendImage(Uint8List imageBuffer) async {
    if (_dataChar == null || _ctrlChar == null) {
      throw Exception('Not connected');
    }

    _setState(TransferState.sending);
    _progressController.add(0.0);

    final size = imageBuffer.length;
    _log('Starting transfer: $size bytes');

    // Send START command
    await _ctrlChar!.write([cmdStart, size & 0xFF, (size >> 8) & 0xFF]);

    // Brief pause for firmware to process START
    await Future.delayed(const Duration(milliseconds: 100));

    // Get effective MTU for chunk sizing
    final mtu = _device!.mtuNow;
    final chunkSize = mtu - 3; // ATT header overhead
    _log('Using chunk size: $chunkSize (MTU: $mtu)');

    // Stream data chunks using write-without-response
    int offset = 0;
    while (offset < size) {
      final end = (offset + chunkSize).clamp(0, size);
      final chunk = imageBuffer.sublist(offset, end);

      await _dataChar!.write(chunk, withoutResponse: true);
      offset = end;

      // Update local progress
      _progressController.add(offset / size);

      // Small delay every ~2KB to avoid BLE congestion
      if (offset % 2048 < chunkSize) {
        await Future.delayed(const Duration(milliseconds: 20));
      }
    }

    _log('All data sent. Committing...');
    await _ctrlChar!.write([cmdCommit]);
  }

  Future<void> disconnect() async {
    if (_device != null) {
      _log('Disconnecting...');
      await _device!.disconnect();
    }
    _cleanup();
    _setState(TransferState.idle);
  }

  void _cleanup() {
    _ctrlNotifySub?.cancel();
    _ctrlNotifySub = null;
    _connectionSub?.cancel();
    _connectionSub = null;
    _dataChar = null;
    _ctrlChar = null;
    _device = null;
  }

  void dispose() {
    _cleanup();
    _stateController.close();
    _progressController.close();
    _logController.close();
  }
}
