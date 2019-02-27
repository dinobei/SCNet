#include <stdlib.h>
#include <iostream>

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <ijoon/coreutils.h>
#include <vector>

#include "packet.pb.h"

using namespace std;

namespace ijoon {
    void printErrorMessage(sql::SQLException &e);
    
    class RendezvousDatabase {
        sql::Driver *driver;
        sql::Connection *con;

    public:
        RendezvousDatabase(std::string db, std::string address, std::string user, std::string pw) {
            try {
                /* Create a connection */
                driver = get_driver_instance();
                con = driver->connect(address, user, pw);
                /* Connect to the MySQL database */
                con->setSchema(db);
            }
            catch (sql::SQLException &e) {
                printErrorMessage(e);
            }
        }
        ~RendezvousDatabase() {
            delete con;
        }

        // RendezvousClient CRUD
        void registrationRendezvousClient(std::string pubIP, std::string pubPort, std::string priIP, std::string priPort, std::string serial = "");
        std::shared_ptr<example::CameraListResponse> getRendezvousClientList();
        std::shared_ptr<ijoon::Peer> getRendezvousClient(std::string ip, std::string port);
        void updateRendezvousClient(std::string ip, std::string port);
        void removeRendezvousClient(std::string ip, std::string port);
    };

}
