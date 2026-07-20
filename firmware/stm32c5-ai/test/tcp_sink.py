#!/usr/bin/env python3
# 간단한 TCP 싱크 서버 - 보드가 접속해서 데이터를 보내면 처리량(Mbps)을 측정한다.
#   사용:  python3 tcp_sink.py [port]         (기본 5001)
#   보드:  iperf tcptx <이 PC IP> [port]
#   종료:  Ctrl-C
import socket, time, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 5001

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", port))
srv.listen(1)
srv.settimeout(1.0)   # accept()가 주기적으로 깨어나 Ctrl-C를 처리하도록
print(f"TCP sink listening on :{port}  (board: iperf tcptx <pc_ip> {port})")
print("Ctrl-C to quit")

try:
    while True:
        try:
            conn, addr = srv.accept()
        except socket.timeout:
            continue   # 연결 대기 중 - 신호 확인 후 다시 대기

        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        print(f"\nconnected from {addr}")
        total = 0
        t0 = time.time()
        last_t, last_b = t0, 0
        try:
            while True:
                data = conn.recv(65536)
                if not data:
                    break
                total += len(data)
                now = time.time()
                if now - last_t >= 1.0:
                    mbps = (total - last_b) * 8 / (now - last_t) / 1e6
                    print(f"  {mbps:6.2f} Mbps   (total {total/1e6:6.1f} MB)")
                    last_t, last_b = now, total
        except Exception as e:
            print("recv error:", e)
        finally:
            conn.close()
        dt = time.time() - t0
        avg = total * 8 / dt / 1e6 if dt > 0 else 0
        print(f"closed: {total} bytes in {dt:.2f}s  =  {avg:.2f} Mbps avg")
except KeyboardInterrupt:
    print("\nbye")
finally:
    srv.close()
