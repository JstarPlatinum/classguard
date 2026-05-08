# MLX90640 V1 Freeze

Date: 2026-05-08

## Summary

MLX90640 V1 is frozen as the current bring-up baseline. The temporary UART thermal data transfer and PC viewer are intentionally kept for continued testing.

## Frozen Behavior

- Sensor: MLX90640 on I2C0, SDA GPIO39, SCL GPIO40, address `0x33`.
- Output: calculated 32x24 temperature matrix in Celsius.
- Frame period: `1000 ms`.
- Test UART: UART0 on GPIO43 TX and GPIO44 RX, `115200 8N1`.
- Test entry: `app_main()` starts `mlx90640_data_test_start()`.
- PC viewer: `tools/mlx90640_viewer/mlx90640_viewer.py`.

## UART Frame

- Magic: `CGTH`.
- Header length: 28 bytes, little-endian.
- Payload: 768 little-endian `float32` values, row-major, Celsius.
- Payload length: 3072 bytes.
- Integrity: CRC32 over payload.
- Debug text remains enabled and may appear between binary frames.

## Validation

- ESP-IDF build passes.
- Viewer self-test passes.
- Hardware test passed with bytes received and thermal image displayed.

## Notes

- This protocol is a V1 test protocol, not the final frozen product protocol.
- Keep the UART transfer and debug prints until the next MLX90640 integration stage replaces them.
- Flash size warning may remain if `sdkconfig` is still set to 2MB on a 16MB board; it is not part of the MLX90640 V1 behavior.
