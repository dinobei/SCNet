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
