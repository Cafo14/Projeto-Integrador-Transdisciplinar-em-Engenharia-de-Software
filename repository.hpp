#pragma once
#include "vendor/json.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <fstream>
#include <memory>
#include <mutex>
using Json=nlohmann::json;

// Repository: o domínio não precisa conhecer detalhes de SQL.
struct OrderRepository {
    virtual ~OrderRepository()=default;
    virtual Json products()=0;
    virtual Json find(const std::string& id)=0;
    virtual Json byKey(const std::string& key)=0;
    virtual void insert(const Json& order)=0;
    virtual void update(const Json& order)=0;
    virtual void reserve(int id,int quantity)=0;
    virtual void release(int id,int quantity)=0;
    virtual int reservations(const std::string& date,const std::string& slot)=0;
    virtual void notify(const Json& order,const std::string& event)=0;
};
class Statement {
    sqlite3_stmt* s=nullptr;
public:
    Statement(sqlite3* db,const char* query){if(sqlite3_prepare_v2(db,query,-1,&s,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
    ~Statement(){sqlite3_finalize(s);}
    Statement(const Statement&)=delete;
    void bind(int i,const std::string& value){if(sqlite3_bind_text(s,i,value.c_str(),-1,SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error("Falha ao vincular texto");}
    void bind(int i,int value){if(sqlite3_bind_int(s,i,value)!=SQLITE_OK)throw std::runtime_error("Falha ao vincular número");}
    bool next(){const int rc=sqlite3_step(s);if(rc==SQLITE_ROW)return true;if(rc==SQLITE_DONE)return false;throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(s)));}
    std::string text(int i){const auto* v=sqlite3_column_text(s,i);return v?reinterpret_cast<const char*>(v):"";}
    int number(int i){return sqlite3_column_int(s,i);}
};
class SQLiteRepository final:public OrderRepository {
    sqlite3* db=nullptr;
public:
    std::mutex mutex;
    explicit SQLiteRepository(const std::string& path,const std::string& seed){
        if(sqlite3_open(path.c_str(),&db)!=SQLITE_OK)throw std::runtime_error("Não foi possível abrir SQLite");
        sqlite3_busy_timeout(db,5000);
        exec("PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON; CREATE TABLE IF NOT EXISTS products(id INTEGER PRIMARY KEY,stock INTEGER NOT NULL CHECK(stock>=0),data TEXT NOT NULL); CREATE TABLE IF NOT EXISTS orders(id TEXT PRIMARY KEY,idempotency TEXT NOT NULL UNIQUE,data TEXT NOT NULL); CREATE TABLE IF NOT EXISTS outbox(id INTEGER PRIMARY KEY AUTOINCREMENT,order_id TEXT NOT NULL REFERENCES orders(id),event TEXT NOT NULL,payload TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'sandbox');");
        std::ifstream stream(seed);if(!stream)throw std::runtime_error("Catálogo inicial não encontrado");Json catalog;stream>>catalog;
        for(const auto& p:catalog){Statement s(db,"INSERT OR IGNORE INTO products(id,stock,data) VALUES(?,?,?)");s.bind(1,p.at("id").get<int>());s.bind(2,p.at("stock").get<int>());s.bind(3,p.dump());s.next();}
    }
    ~SQLiteRepository(){sqlite3_close(db);}
    void exec(const char* sql){char* error=nullptr;if(sqlite3_exec(db,sql,nullptr,nullptr,&error)!=SQLITE_OK){std::string message=error?error:"Erro SQLite";sqlite3_free(error);throw std::runtime_error(message);}}
    Json products() override {Statement s(db,"SELECT data,stock FROM products ORDER BY id");Json result=Json::array();while(s.next()){auto p=Json::parse(s.text(0));p["stock"]=s.number(1);result.push_back(p);}return result;}
    Json find(const std::string& id) override {Statement s(db,"SELECT data FROM orders WHERE id=?");s.bind(1,id);return s.next()?Json::parse(s.text(0)):Json();}
    Json byKey(const std::string& key) override {Statement s(db,"SELECT data FROM orders WHERE idempotency=?");s.bind(1,key);return s.next()?Json::parse(s.text(0)):Json();}
    void insert(const Json& o) override {Statement s(db,"INSERT INTO orders(id,idempotency,data) VALUES(?,?,?)");s.bind(1,o.at("id").get<std::string>());s.bind(2,o.at("idempotency").get<std::string>());s.bind(3,o.dump());s.next();}
    void update(const Json& o) override {Statement s(db,"UPDATE orders SET data=? WHERE id=?");s.bind(1,o.dump());s.bind(2,o.at("id").get<std::string>());s.next();}
    void reserve(int id,int q) override {Statement s(db,"UPDATE products SET stock=stock-? WHERE id=? AND stock>=?");s.bind(1,q);s.bind(2,id);s.bind(3,q);s.next();if(sqlite3_changes(db)!=1)throw std::invalid_argument("Estoque insuficiente. Revise a sacola.");}
    void release(int id,int q) override {Statement s(db,"UPDATE products SET stock=stock+? WHERE id=?");s.bind(1,q);s.bind(2,id);s.next();}
    int reservations(const std::string& date,const std::string& slot) override {Statement s(db,"SELECT COUNT(*) FROM orders WHERE json_extract(data,'$.date')=? AND json_extract(data,'$.slot')=? AND json_extract(data,'$.paymentStatus')!='Recusado'");s.bind(1,date);s.bind(2,slot);s.next();return s.number(0);}
    void notify(const Json& order,const std::string& event) override {Statement s(db,"INSERT INTO outbox(order_id,event,payload) VALUES(?,?,?)");s.bind(1,order.at("id").get<std::string>());s.bind(2,event);s.bind(3,Json{{"to",order.at("email")},{"order",order.at("id")},{"event",event}}.dump());s.next();}
};
// RAII / Unit of Work: estoque, pedido e outbox são confirmados juntos.
class Transaction {
    SQLiteRepository& db;bool committed=false;
public:
    explicit Transaction(SQLiteRepository& db):db(db){db.exec("BEGIN IMMEDIATE");}
    void commit(){db.exec("COMMIT");committed=true;}
    ~Transaction(){if(!committed){try{db.exec("ROLLBACK");}catch(...){}}}
};
