import time
import numpy as np
import serial

# SERIAL SETTINGS
PORT = "COM14"
BAUD = 921600

SERVO_PORT = "COM8"
SERVO_BAUD = 115200

# DSP SETTINGS
FS = 16000
N = 2048
C = 343.0

RMS_MIN = 3000
RMS_NOISE_FLOOR = 500.0


def main():
    ser = serial.Serial(
        PORT,
        BAUD,
        timeout=1
    )

    time.sleep(2)
    ser.reset_input_buffer()

    servo_ser = serial.Serial(
        SERVO_PORT,
        SERVO_BAUD,
        timeout=0.1
    )

    time.sleep(2)
    servo_ser.reset_input_buffer()

    print()
    print("=" * 90)
    print("3-MIC SOUND LOCALIZATION")
    print("RMS + GCC-PHAT TDOA")
    print("PROCESSING LATENCY MEASUREMENT ENABLED")
    print("=" * 90)

    print("Mic 1 = 0°")
    print("Mic 2 = 90°")
    print("Mic 3 = 180°")

    print("=" * 90)

    print(f"ESP32 AUDIO : {PORT}")
    print(f"ARDUINO UNO : {SERVO_PORT}")
    print()

    hist = []
    last_servo_angle = None

    # Processing latency statistics
    latency_values = []
    latency_count = 0
    latency_total = 0.0
    latency_max = 0.0

    try:
        while True:
            x = read_block(ser)

            if x is None:
                continue

            x = x[:, COLS]

            # Clipping check
            clip = (
                np.mean(
                    np.abs(x) >= 32767,
                    axis=0
                ) * 100.0
            )

            # Remove DC
            x = x - x.mean(axis=0)

            # Individual microphone RMS
            rms = np.sqrt(
                np.mean(x ** 2, axis=0)
            )

            if rms.max() < RMS_MIN:
                continue

            if clip.max() > 1.0:
                print(
                    f"RMS {rms[0]:6.0f} "
                    f"{rms[1]:6.0f} "
                    f"{rms[2]:6.0f} | CLIPPING"
                )
                continue

            # ==========================================
            # START DSP PROCESSING LATENCY TIMER
            # ==========================================
            process_start = time.perf_counter()

            # Windowing
            window = np.hanning(N)
            xw = x * window[:, None]

            # GCC-PHAT for all microphone pairs
            meas = {}
            pair_conf = {}

            for a, b, key in PAIRS:
                tau, confidence = gcc_phat(
                    xw[:, b],
                    xw[:, a],
                    MAX_TAU[key]
                )

                meas[key] = tau - 0.0
                pair_conf[key] = confidence

            conf = float(np.mean(list(pair_conf.values())))

            # TDOA angle
            tdoa_angle, pair_weights = estimate_tdoa_angle(
                meas,
                pair_conf
            )

            # RMS angle
            rms_ang, weights = rms_angle(rms)

            # TDOA reliability
            tdoa_quality = calculate_tdoa_quality(
                pair_conf,
                meas
            )

            # Adaptive fusion
            (
                final_angle,
                rms_w,
                tdoa_w,
                dominance
            ) = fuse_angles(
                rms_ang,
                tdoa_angle,
                rms,
                tdoa_quality
            )

            # Median smoothing
            hist.append(final_angle)

            if len(hist) > HISTORY_LENGTH:
                hist.pop(0)

            median_angle = float(np.median(hist))

            # ==========================================
            # STOP DSP PROCESSING LATENCY TIMER
            # ==========================================
            process_end = time.perf_counter()

            processing_latency_ms = (
                process_end - process_start
            ) * 1000.0

            # Update statistics
            latency_count += 1
            latency_total += processing_latency_ms
            latency_values.append(processing_latency_ms)

            latency_max = max(
                latency_max,
                processing_latency_ms
            )

            average_latency = latency_total / latency_count

            # Servo angle
            servo_angle = correct_servo_angle(median_angle)

            if (
                last_servo_angle is None or
                abs(servo_angle - last_servo_angle) >= 2
            ):
                servo_ser.write(
                    f"ANGLE:{servo_angle}\n".encode()
                )
                last_servo_angle = servo_angle

            # Terminal output
            print()

            print(
                f"ANGLE = {final_angle:6.1f}°   "
                f"MEDIAN = {median_angle:6.1f}°   "
                f"SERVO = {servo_angle:3d}°"
            )

            print(
                f"RMS-A = {rms_ang:6.1f}°   "
                f"TDOA-A = {tdoa_angle:6.1f}°"
            )

            print(
                f"RMS  : "
                f"{rms[0]:6.0f} "
                f"{rms[1]:6.0f} "
                f"{rms[2]:6.0f}"
            )

            print(
                f"RMS-W: "
                f"{weights[0]:.2f} "
                f"{weights[1]:.2f} "
                f"{weights[2]:.2f}"
            )

            print(
                f"WEIGHT: "
                f"RMS {rms_w:.2f} | "
                f"TDOA {tdoa_w:.2f} | "
                f"DOM {dominance:.2f}"
            )

            print(
                f"TDOA: "
                f"12={meas['12'] * 1e6:6.0f} us  "
                f"13={meas['13'] * 1e6:6.0f} us  "
                f"23={meas['23'] * 1e6:6.0f} us"
            )

            print(f"CONF = {conf:.2f}")

            print(
                f"PROCESSING LATENCY = "
                f"{processing_latency_ms:.3f} ms"
            )

            print(
                f"AVERAGE LATENCY = "
                f"{average_latency:.3f} ms"
            )

            print("-" * 90)

    except KeyboardInterrupt:
        print("\nLocalization stopped.")

    finally:
        print("\n" + "=" * 50)
        print("PROCESSING LATENCY SUMMARY")
        print("=" * 50)

        if latency_count > 0:
            print(f"Processed blocks : {latency_count}")
            print(f"Average latency  : {latency_total / latency_count:.3f} ms")
            print(f"Maximum latency  : {latency_max:.3f} ms")
        else:
            print("No blocks were processed.")

        print("=" * 50)

        ser.close()
        servo_ser.close()


if __name__ == "__main__":
    main()