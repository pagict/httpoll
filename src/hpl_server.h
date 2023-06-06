
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "hpl_connection.h"
#include "hpl_request_handler.h"
namespace hpl {

class Connection;

class Server {
public:
  explicit Server();
  int Init(const char *addr, int port, int backlog);

  /// @param timeout, in milliseconds, -1 == infinite
  int Poll(int timeout);
  int RegisterRequestHandler(const std::string &uri, RequestHandler &&handler);

  int CloseConn(Connection *conn);

private:
  int server_fd;
  int poll_fd;
  std::unique_ptr<Connection> server_conn_;

  std::list<std::unique_ptr<Connection>> pending_conns_;

  std::unique_ptr<Connection> AcceptNewConnection();

  std::map<std::string, RequestHandler, std::less<>> request_handlers;

  decltype(request_handlers)::const_iterator
  FindRequestHandler(std::string_view uri_view) const;
};
} // namespace hpl