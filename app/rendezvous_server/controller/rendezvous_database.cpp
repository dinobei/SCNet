#include "rendezvous_database.h"

void ijoon::printErrorMessage(sql::SQLException &e) {
    cout << "# ERR: SQLException in " << __FILE__;
    cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
    cout << "# ERR: " << e.what();
    cout << " (MySQL error code: " << e.getErrorCode();
    cout << ", SQLState: " << e.getSQLState() << " )" << endl;
}

void ijoon::RendezvousDatabase::registrationRendezvousClient(std::string pubIP, std::string pubPort, std::string priIP,
                                                             std::string priPort, std::string serial) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("INSERT INTO RendezvousClient(serial, pub_ip, pub_port, pri_ip, pri_port, ping) VALUES(?, ?, ?, ?, ?, ?)"));
        prep_stmt->setString(1, serial);
        prep_stmt->setString(2, pubIP);
        prep_stmt->setString(3, pubPort);
        prep_stmt->setString(4, priIP);
        prep_stmt->setString(5, priPort);
        prep_stmt->setDateTime(6, *ijoon::ComputableTime::getCurrentTimeText().get());
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}

std::shared_ptr<example::CameraListResponse> ijoon::RendezvousDatabase::getRendezvousClientList() {
    std::shared_ptr<sql::Statement> stmt;
    std::shared_ptr<sql::ResultSet> res;

    auto cameraListResponse = std::shared_ptr<example::CameraListResponse>(new example::CameraListResponse());
    
    try {
        stmt = std::shared_ptr<sql::Statement>(con->createStatement());
        res = std::shared_ptr<sql::ResultSet>(stmt->executeQuery("SELECT * from RendezvousClient"));
        
        while(res->next()) {
            auto camera = cameraListResponse->add_cameralist();
            camera->set_name("unknown");
            camera->set_serial(res->getString("serial"));
            camera->set_publicip(res->getString("pub_ip"));
            camera->set_publicport(res->getString("pub_port"));
            camera->set_privateip(res->getString("pri_ip"));
            camera->set_privateport(res->getString("pri_port"));
            
            ijoon::ComputableTime time(res->getString("ping"));
            camera->set_ping(time.getTimeSec());
        }
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
    
    return cameraListResponse;
}

std::shared_ptr<ijoon::Peer> ijoon::RendezvousDatabase::getRendezvousClient(std::string ip, std::string port) {
    std::shared_ptr<sql::ResultSet> res;
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("SELECT pri_ip, pri_port from RendezvousClient WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setString(1, ip);
        prep_stmt->setString(2, port);
        res = std::shared_ptr<sql::ResultSet>(prep_stmt->executeQuery());
        
        while(res->next()) {
            auto peer = std::shared_ptr<ijoon::Peer>(new ijoon::Peer(res->getString(1), res->getString(2)));
            return peer;
        }
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
    
    return nullptr;
}

void ijoon::RendezvousDatabase::updateRendezvousClient(std::string ip, std::string port) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("UPDATE RendezvousClient SET ping=? WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setDateTime(1, *ijoon::ComputableTime::getCurrentTimeText().get());
        prep_stmt->setString(2, ip);
        prep_stmt->setString(3, port);
        
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}

void ijoon::RendezvousDatabase::removeRendezvousClient(std::string ip, std::string port) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("DELETE FROM RendezvousClient WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setString(1, ip);
        prep_stmt->setString(2, port);
        
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}

void ijoon::RendezvousDatabase::registrationRelayServer(std::string name, std::string pubIP, std::string pubPort, std::string version) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("INSERT INTO RelayServer(name, pub_ip, pub_port, ping, ver) VALUES(?, ?, ?, ?, ?)"));
        prep_stmt->setString(1, name);
        prep_stmt->setString(2, pubIP);
        prep_stmt->setString(3, pubPort);
        prep_stmt->setString(4, *ijoon::ComputableTime::getCurrentTimeText().get());
        prep_stmt->setString(5, version);
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}

std::shared_ptr<std::vector<std::shared_ptr<ijoon::RelayServerModel>>> ijoon::RendezvousDatabase::getRelayServerList() {
    std::shared_ptr<sql::Statement> stmt;
    std::shared_ptr<sql::ResultSet> res;
    
    auto relayServerList = std::shared_ptr<std::vector<std::shared_ptr<ijoon::RelayServerModel>>>(new std::vector<std::shared_ptr<ijoon::RelayServerModel>>());
    
    try {
        stmt = std::shared_ptr<sql::Statement>(con->createStatement());
        res = std::shared_ptr<sql::ResultSet>(stmt->executeQuery("SELECT * from RelayServer"));
        
        while(res->next()) {
            auto relayServer = std::shared_ptr<ijoon::RelayServerModel>(new ijoon::RelayServerModel());
            
            relayServer->publicIP = res->getString("pub_ip");
            relayServer->publicPort = res->getString("pub_port");
            ijoon::ComputableTime time(res->getString("ping"));
            relayServer->ping = time.getTimeSec();
            
            relayServerList->push_back(relayServer);
        }
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
    
    return relayServerList;
}

std::shared_ptr<ijoon::RelayServerModel> ijoon::RendezvousDatabase::getRelayServer(std::string ip, std::string port) {
    std::shared_ptr<sql::ResultSet> res;
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("SELECT pub_ip, pub_port, ping from RelayServer WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setString(1, ip);
        prep_stmt->setString(2, port);
        res = std::shared_ptr<sql::ResultSet>(prep_stmt->executeQuery());
        
        while(res->next()) {
            auto relayServer = std::shared_ptr<ijoon::RelayServerModel>(new ijoon::RelayServerModel());
            relayServer->publicIP = res->getString(1);
            relayServer->publicPort = res->getString(2);
            
            ijoon::ComputableTime time(res->getString(3));
            relayServer->ping = time.getTimeSec();
            return relayServer;
        }
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
    
    return nullptr;
}

void ijoon::RendezvousDatabase::updateRelayServer(std::string ip, std::string port) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("UPDATE RelayServer SET ping=? WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setDateTime(1, *ijoon::ComputableTime::getCurrentTimeText().get());
        prep_stmt->setString(2, ip);
        prep_stmt->setString(3, port);
        
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}

void ijoon::RendezvousDatabase::removeRelayServer(std::string ip, std::string port) {
    std::shared_ptr<sql::PreparedStatement> prep_stmt;
    
    try {
        prep_stmt = std::shared_ptr<sql::PreparedStatement>(con->prepareStatement("DELETE FROM RelayServer WHERE pub_ip=? AND pub_port=?"));
        prep_stmt->setString(1, ip);
        prep_stmt->setString(2, port);
        
        prep_stmt->execute();
    }
    catch(sql::SQLException &e) {
        printErrorMessage(e);
    }
}
