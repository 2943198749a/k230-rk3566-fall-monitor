import os
import socket
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

CLIENT = Path(__file__).resolve().parents[1] / "build" / "k230_status_client"


def line_from(sock):
    data = bytearray()
    while not data.endswith(b"\n"):
        chunk = sock.recv(1)
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def assert_qt_accepts(line):
    probe = os.environ.get("QT_STATUS_PROBE")
    if probe:
        subprocess.run([probe, line.decode("ascii").strip()], check=True, stdout=subprocess.DEVNULL)


class StatusClientTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "boxes"
        self.server = socket.socket()
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind(("127.0.0.1", 0))
        self.server.listen(3)
        self.server.settimeout(3)
        self.proc = subprocess.Popen([str(CLIENT), "127.0.0.1", str(self.server.getsockname()[1]), str(self.path), "100", "700"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def tearDown(self):
        self.proc.terminate()
        self.proc.wait(timeout=4)
        self.server.close()
        self.tmp.cleanup()

    def publish(self, value):
        temp = self.path.with_suffix(".tmp")
        temp.write_text(value, encoding="ascii")
        os.replace(temp, self.path)

    def test_format_ack_and_stale_disconnect(self):
        self.publish("0 0.8 10 20 100 200\n1 0.7 20 30 300 400\n")
        conn, _ = self.server.accept()
        conn.settimeout(3)
        with conn:
            line = line_from(conn)
            self.assertEqual(line, b"STATUS seq=1 total=2 standing=1 fallen=1 state=FALL\n")
            assert_qt_accepts(line)
            conn.sendall(b"ACK se")
            conn.sendall(b"q=1\n")
            self.publish("")
            line = line_from(conn)
            self.assertEqual(line, b"STATUS seq=2 total=0 standing=0 fallen=0 state=SAFE\n")
            assert_qt_accepts(line)
            conn.sendall(b"ACK seq=2\n")
            # Do not refresh the producer. ACK the last repeats until the file
            # ages out; the client must close instead of reporting stale SAFE.
            end = time.monotonic() + 3
            while time.monotonic() < end:
                line = line_from(conn)
                if not line:
                    break
                seq = line.split(b"seq=")[1].split()[0]
                conn.sendall(b"ACK seq=" + seq + b"\n")
            else:
                self.fail("client did not close after detection data became stale")

    def test_missing_or_malformed_snapshot_never_connects(self):
        self.publish("0 nan 1 2 3 4\n")
        self.server.settimeout(1)
        with self.assertRaises(socket.timeout):
            self.server.accept()

    def test_reconnect_after_server_disconnect(self):
        self.publish("1 0.8 10 20 100 200\n")
        conn, _ = self.server.accept()
        conn.settimeout(2)
        self.assertIn(b"standing=1 fallen=0 state=SAFE", line_from(conn))
        conn.close()
        # The camera keeps producing fresh snapshots while the network reconnects.
        end = time.monotonic() + 4
        self.server.settimeout(0.2)
        while time.monotonic() < end:
            self.publish("1 0.8 10 20 100 200\n")
            try:
                conn, _ = self.server.accept()
                conn.settimeout(2)
                with conn:
                    self.assertIn(b"standing=1 fallen=0 state=SAFE", line_from(conn))
                return
            except socket.timeout:
                pass
        self.fail("client did not reconnect")

    def test_wrong_ack_closes_connection(self):
        self.publish("1 0.8 10 20 100 200\n")
        conn, _ = self.server.accept()
        conn.settimeout(3)
        with conn:
            self.assertTrue(line_from(conn).startswith(b"STATUS seq=1 "))
            conn.sendall(b"ACK seq=999\n")
            self.assertEqual(conn.recv(1), b"")

    def test_no_arguments_uses_default_endpoint(self):
        process = subprocess.Popen([str(CLIENT)], stdout=subprocess.DEVNULL,
                                   stderr=subprocess.PIPE, text=True)
        try:
            self.assertIn("192.168.10.3:9000", process.stderr.readline())
            self.assertIsNone(process.poll())
        finally:
            process.terminate()
            process.wait(timeout=4)
            process.stderr.close()


if __name__ == "__main__":
    unittest.main()
