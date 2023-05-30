#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdio.h>

#include <chrono>
#include <numeric>

FILE *log_file = nullptr;

struct Context {
  Context(int n_conns, int nbytes) : n_conns_(n_conns), nbytes(nbytes) {
    const unsigned kFmtErrBufSize = 64;
    char fmterrbuf[kFmtErrBufSize];
    buf_ = new char[nbytes + 1];
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_un serv_addr;
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, "/tmp/broadcast_socket");
    unlink(serv_addr.sun_path);

    // struct sockaddr_in serv_addr;
    // serv_addr.sin_family = AF_INET;
    // serv_addr.sin_port = htons(8080);
    // serv_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(server_fd, n_conns_);
    client_fds = new int[n_conns_];
    struct sockaddr_un client_addrs;
    client_addrs.sun_family = AF_UNIX;
    strcpy(client_addrs.sun_path, "/tmp/broadcast_socket");
    socklen_t client_addrs_len = sizeof(client_addrs);

    // struct sockaddr_in client_addrs;
    // socklen_t client_addrs_len = sizeof(client_addrs);
    // client_addrs.sin_family = AF_INET;
    // client_addrs.sin_port = htons(8080);
    // inet_pton(AF_INET, "127.0.0.1", &client_addrs.sin_addr);
    int rmem = nbytes;
    for (int i = 0; i < n_conns_; i++) {
      client_fds[i] = socket(AF_UNIX, SOCK_STREAM, 0);
      if (client_fds[i] < 0) {
        fprintf(stderr, "client create socket err: %s\n",
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (connect(client_fds[i], (struct sockaddr *)&client_addrs,
                  client_addrs_len) < 0) {
        fprintf(stderr, "client connect err: %s\n",
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(client_fds[i], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "client connect fcntl nonblock err: %s\n",
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (setsockopt(client_fds[i], SOL_SOCKET, SO_RCVBUF, &rmem,
                     sizeof(rmem)) < 0) {
        fprintf(stderr, "client setsockopt SO_RCVBUF(%d) err: %s\n", rmem,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
    }

    accepted_fds = new int[n_conns_];
    int wmem = nbytes;
    for (int i = 0; i < n_conns_; i++) {
      accepted_fds[i] = accept(server_fd, nullptr, nullptr);
      if (accepted_fds[i] < 0) {
        fprintf(stderr, "server accept err: %s\n",
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(accepted_fds[i], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "server accept fcntl nonblock err: %s\n",
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (setsockopt(accepted_fds[i], SOL_SOCKET, SO_SNDBUF, &wmem,
                     sizeof(wmem)) < 0) {
        fprintf(stderr, "server setsockopt SO_SNDBUF(%d) err: %s\n", wmem,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
    }

    pipe(pipes);
    if (fcntl(pipes[0], F_SETFL, O_NONBLOCK) < 0) {
      fprintf(stderr, "pipe[0] fcntl nonblock err: %s\n",
              strerror_r(errno, fmterrbuf, kFmtErrBufSize));
      exit(1);
    }
    if (fcntl(pipes[1], F_SETFL, O_NONBLOCK) < 0) {
      fprintf(stderr, "pipe[1] fcntl nonblock err: %s\n",
              strerror_r(errno, fmterrbuf, kFmtErrBufSize));
      exit(1);
    }
    if (fcntl(pipes[0], F_SETPIPE_SZ, nbytes) < 0) {
      fprintf(stderr, "pipe[0] fcntl set pipe size err: %s\n",
              strerror_r(errno, fmterrbuf, kFmtErrBufSize));
      exit(1);
    }
    if (fcntl(pipes[1], F_SETPIPE_SZ, nbytes) < 0) {
      fprintf(stderr, "pipe[1] fcntl set pipe size err: %s\n",
              strerror_r(errno, fmterrbuf, kFmtErrBufSize));
      exit(1);
    }

    conn_pipes = new int *[n_conns_];
    for (auto i = 0; i < n_conns_; ++i) {
      conn_pipes[i] = new int[2];
      if (pipe(conn_pipes[i]) < 0) {
        fprintf(stderr, "conn_pipes[%d] pipe err: %s\n", i,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(conn_pipes[i][0], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "conn_pipes[%d][0] fcntl nonblock err: %s\n", i,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(conn_pipes[i][1], F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr, "conn_pipes[%d][1] fcntl nonblock err: %s\n", i,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(conn_pipes[i][0], F_SETPIPE_SZ, nbytes) < 0) {
        fprintf(stderr, "conn_pipes[%d][0] fcntl set pipe size err: %s\n", i,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
      if (fcntl(conn_pipes[i][1], F_SETPIPE_SZ, nbytes) < 0) {
        fprintf(stderr, "conn_pipes[%d][1] fcntl set pipe size err: %s\n", i,
                strerror_r(errno, fmterrbuf, kFmtErrBufSize));
        exit(1);
      }
    }
  }

  void DirectWrite() {
    const unsigned kBufSize = 64;
    char errbuf[kBufSize];
    int ret = 0;
    for (int i = 0; i < n_conns_; i++) {
      ret = write(accepted_fds[i], buf_, nbytes);
      if (ret < 0) {
        fprintf(log_file, "%d %d direct write err: %s\n", n_conns_, nbytes,
                strerror_r(errno, errbuf, kBufSize));
      }
    }
  }

  void ZeroCopy() {
    const unsigned kBufSize = 64;
    char errbuf[kBufSize];
    int ret = write(pipes[1], buf_, nbytes);
    if (ret < 0) {
      fprintf(log_file, "%d %d zerocopy write err: %s\n", n_conns_, nbytes,
              strerror_r(errno, errbuf, kBufSize));
    }
    for (auto i = 0; i < n_conns_ - 1; ++i) {
      ret = tee(pipes[0], conn_pipes[i][1], nbytes, 0);
      if (ret < 0) {
        fprintf(log_file, "%d %d zerocopy tee err: %s\n", n_conns_, nbytes,
                strerror_r(errno, errbuf, kBufSize));
      }
    }
    ret = splice(pipes[0], 0, conn_pipes[n_conns_ - 1][1], 0, nbytes, 0);
    if (ret < 0) {
      fprintf(log_file, "%d %d zerocopy splice for pipe err: %s\n", n_conns_,
              nbytes, strerror_r(errno, errbuf, kBufSize));
    }

    for (auto i = 0; i < n_conns_; ++i) {
      ret = splice(conn_pipes[i][0], 0, accepted_fds[i], 0, nbytes, 0);
      if (ret < 0) {
        fprintf(log_file, "%d %d zerocopy splice per conn err: %s\n", n_conns_,
                nbytes, strerror_r(errno, errbuf, kBufSize));
      }
    }
  }

  ~Context() {
    for (int i = 0; i < n_conns_; i++) {
      close(accepted_fds[i]);
      close(client_fds[i]);
      close(conn_pipes[i][0]);
      close(conn_pipes[i][1]);

      delete[] conn_pipes[i];
    }
    delete[] conn_pipes;
    close(pipes[0]);
    close(pipes[1]);
    close(server_fd);
    delete[] buf_;
    delete[] client_fds;
    delete[] accepted_fds;
  }

  void Drain() {
    int ret = 0;
    for (int i = 0; i < n_conns_; ++i) {
      ret = read(client_fds[i], buf_, nbytes);
      if (ret < 0) {
        fprintf(log_file, "%d %d drain err: %s\n", n_conns_, nbytes,
                strerror_r(errno, buf_, nbytes));
      }
    }
  }

  int n_conns_;
  int nbytes;
  char *buf_;
  int server_fd;
  int *client_fds;
  int *accepted_fds;
  int **conn_pipes;

  int pipes[2];
};

int main(int argc, char **argv) {
  int n_conns = 5;
  int nbytes = 1024 * 10;
  log_file = stderr;
  if (argc > 1) {
    n_conns = atoi(argv[1]);
  }
  if (argc > 2) {
    nbytes = atoi(argv[2]);
    char unit = argv[2][strlen(argv[2]) - 1];
    if (unit == 'k' || unit == 'K') {
      nbytes *= 1024;
    } else if (unit == 'm' || unit == 'M') {
      nbytes *= 1024 * 1024;
    }
  }

  FILE *out = stdout;
  if (argc > 3) {
    out = fopen(argv[3], "a+");
  }
  if (argc > 4) {
    log_file = fopen(argv[4], "a+");
  }

  Context ctx(n_conns, nbytes);
  const unsigned kIteration = 100;
  int zero_copy_time[kIteration];
  int direct_write_time[kIteration];
  for (auto i = 0; i < 100; ++i) {
    // printf("init at iteration %d...\n", i);
    auto t1 = std::chrono::high_resolution_clock::now();
    ctx.ZeroCopy();
    auto t2 = std::chrono::high_resolution_clock::now();
    ctx.Drain();

    auto t3 = std::chrono::high_resolution_clock::now();
    ctx.DirectWrite();
    auto t4 = std::chrono::high_resolution_clock::now();
    ctx.Drain();

    zero_copy_time[i] =
        std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    direct_write_time[i] =
        std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
  }

  auto zc_avg =
      std::accumulate(zero_copy_time, zero_copy_time + kIteration, 0) /
      kIteration;
  auto dw_avg =
      std::accumulate(direct_write_time, direct_write_time + kIteration, 0) /
      kIteration;
  fprintf(out, "conns %d bytes %d zero_copy %u direct_write %u\n", n_conns,
          nbytes, zc_avg, dw_avg);
  return 0;
}