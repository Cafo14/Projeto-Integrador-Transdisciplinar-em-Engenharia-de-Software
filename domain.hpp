#pragma once
#include "repository.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <random>
#include <regex>
#include <map>
#include <algorithm>

inline std::string randomHex(){std::random_device device;std::uniform_int_distribution<unsigned> dist(0,255);std::ostringstream out;for(int i=0;i<24;i++)out<<std::hex<<std::setfill('0')<<std::setw(2)<<dist(device);return out.str();}
inline std::string dateAt(int days){auto now=std::time(nullptr)-3*3600+days*86400;std::tm tm{};gmtime_r(&now,&tm);char buf[11];std::strftime(buf,sizeof(buf),"%Y-%m-%d",&tm);return buf;}
inline std::string timestamp(){auto now=std::time(nullptr);std::tm tm{};gmtime_r(&now,&tm);char buf[25];std::strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%SZ",&tm);return buf;}
inline std::string field(const Json& j,const std::string& key,size_t max,bool required=true){
    if(!j.contains(key)||!j.at(key).is_string()){if(!required)return "";throw std::invalid_argument("Campo obrigatório: "+key);}
    const auto s=j.at(key).get<std::string>();
    if((required&&s.find_first_not_of(" \t\n\r")==std::string::npos)||s.size()>max||s.find('\0')!=std::string::npos)throw std::invalid_argument("Campo inválido: "+key);
    return s;
}
inline size_t unicodeLength(const std::string& s){return std::count_if(s.begin(),s.end(),[](unsigned char c){return (c&0xc0)!=0x80;});}
// Strategy: o cálculo de frete pode mudar sem alterar o serviço de pedidos.
struct ShippingStrategy {virtual ~ShippingStrategy()=default;virtual Json quote(const std::string& cep)const=0;};
class SaoPauloShipping final:public ShippingStrategy {
public:Json quote(const std::string& cep)const override {if(!std::regex_match(cep,std::regex("[0-9]{8}")))throw std::invalid_argument("CEP deve conter 8 dígitos.");int region=std::stoi(cep.substr(0,2));if(region<1||region>5)throw std::invalid_argument("Entregas disponíveis para CEPs de 01000-000 a 05999-999.");return{{"shipping",region<=2?800:1200},{"estimate",region<=2?"Até 60 minutos dentro da janela escolhida":"Até 90 minutos dentro da janela escolhida"}};}
};
// Strategy + Factory: adaptadores reais podem implementar a mesma interface.
struct PaymentStrategy {virtual ~PaymentStrategy()=default;virtual Json initiate(const std::string& id)const=0;};
class SandboxPayment final:public PaymentStrategy {
    std::string method;
public:explicit SandboxPayment(std::string method):method(std::move(method)){}
    Json initiate(const std::string& id)const override {return{{"paymentStatus","Pendente"},{"pixCode",method=="pix"?"DEMONSTRACAO-SEM-VALOR-FINANCEIRO:"+id:""}};}
};
struct PaymentFactory {
    static std::unique_ptr<PaymentStrategy> create(const std::string& method){if(method!="pix"&&method!="credit"&&method!="debit"&&method!="delivery")throw std::invalid_argument("Forma de pagamento inválida.");return std::make_unique<SandboxPayment>(method);}
};
// Observer: confirmação e entrega geram eventos na outbox, na mesma transação.
struct OrderObserver {virtual ~OrderObserver()=default;virtual void onEvent(const Json&,const std::string&)=0;};
class OutboxObserver final:public OrderObserver {
    OrderRepository& repository;
public:explicit OutboxObserver(OrderRepository& r):repository(r){}void onEvent(const Json& o,const std::string& event)override{repository.notify(o,event);}
};
// State: transições permitidas são centralizadas e não vêm do cliente.
struct DeliveryState {
    static std::string next(const Json& o){if(o.at("paymentStatus")=="Recusado")throw std::invalid_argument("Pedido cancelado.");if(o.at("paymentStatus")!="Aprovado"&&o.at("payment")!="delivery")throw std::invalid_argument("Pagamento pendente.");const auto status=o.at("status").get<std::string>();if(status=="Recebido")return "Em preparo";if(status=="Em preparo")return "Saiu para entrega";if(status=="Saiu para entrega")return "Entregue";throw std::invalid_argument("Pedido já entregue.");}
};
class OrderService {
    SQLiteRepository& db;ShippingStrategy& shipping;OrderObserver& observer;
public:
    OrderService(SQLiteRepository& db,ShippingStrategy& shipping,OrderObserver& observer):db(db),shipping(shipping),observer(observer){}
    Json slots(const std::string& date){Json result=Json::array();bool valid=false;for(int i=1;i<=14;i++)if(date==dateAt(i))valid=true;if(!valid)return result;for(auto time:{"09:00–12:00","12:00–15:00","15:00–18:00"})result.push_back({{"time",time},{"available",std::string(time)!="12:00–15:00"&&db.reservations(date,time)<10}});return result;}
    Json create(const Json& input,const std::string& key){
        if(!input.is_object())throw std::invalid_argument("Pedido inválido.");
        if(!std::regex_match(key,std::regex("[a-zA-Z0-9-]{32,80}")))throw std::invalid_argument("Chave de idempotência inválida.");
        Transaction tx(db);auto previous=db.byKey(key);if(!previous.is_null()){if(previous.at("request")!=input)throw std::invalid_argument("Chave de idempotência já usada com outro pedido.");tx.commit();return previous;}
        Json o;for(auto name:{"name","email","cep","street","number","date","slot","payment"})o[name]=field(input,name,name==std::string("email")?120:240);
        o["complement"]=field(input,"complement",240,false);o["notes"]=field(input,"notes",1200,false);if(unicodeLength(o["notes"].get<std::string>())>300)throw std::invalid_argument("Observações limitadas a 300 caracteres.");
        if(!std::regex_match(o["email"].get<std::string>(),std::regex("[^[:space:]@]+@[^[:space:]@]+\\.[^[:space:]@]+")))throw std::invalid_argument("E-mail inválido.");
        bool available=false;for(const auto& s:slots(o["date"]))if(s["time"]==o["slot"]&&s["available"]==true)available=true;if(!available)throw std::invalid_argument("Data ou horário indisponível.");
        const auto pay=PaymentFactory::create(o["payment"]);o["shipping"]=shipping.quote(o["cep"])["shipping"];
        if(!input.contains("items")||!input["items"].is_array()||input["items"].empty()||input["items"].size()>50)throw std::invalid_argument("Sacola inválida.");
        std::map<int,int> quantities;for(const auto& i:input["items"]){if(!i.is_object()||!i.contains("id")||!i.contains("quantity")||!i["id"].is_number_integer()||!i["quantity"].is_number_integer())throw std::invalid_argument("Item inválido.");const auto id=i["id"].get<long long>(),q=i["quantity"].get<long long>();if(id<1||id>100000||q<1||q>99)throw std::invalid_argument("Quantidade deve ser de 1 a 99.");quantities[static_cast<int>(id)]+=static_cast<int>(q);if(quantities[id]>99)throw std::invalid_argument("Limite de 99 unidades por produto.");}
        const auto catalog=db.products();o["items"]=Json::array();int total=o["shipping"];
        for(const auto& [id,q]:quantities){auto it=std::find_if(catalog.begin(),catalog.end(),[productId=id](const Json& p){return p["id"]==productId;});if(it==catalog.end())throw std::invalid_argument("Produto não encontrado.");db.reserve(id,q);const int price=it->at("price");total+=price*q;o["items"].push_back({{"id",id},{"quantity",q},{"name",it->at("name")},{"price",price}});}
        o["total"]=total;o["id"]="DA-"+randomHex();o["token"]=randomHex();o["idempotency"]=key;o["status"]="Recebido";o["createdAt"]=timestamp();o["notification"]="Confirmação registrada na caixa de saída de teste";o["request"]=input;
        o.update(pay->initiate(o["id"]));db.insert(o);observer.onEvent(o,"order.created");tx.commit();return o;
    }
    Json authorized(const std::string& id,const std::string& bearer){auto o=db.find(id);if(o.is_null()||bearer!="Bearer "+o.at("token").get<std::string>())throw std::out_of_range("Pedido não encontrado.");return o;}
    Json simulate(const std::string& id,const std::string& bearer,const std::string& event){Transaction tx(db);auto o=authorized(id,bearer);if(event=="approve"||event=="reject"){if(o["paymentStatus"]!="Pendente"||o["payment"]=="delivery")throw std::invalid_argument("Pagamento já processado ou pago na entrega.");o["paymentStatus"]=event=="approve"?"Aprovado":"Recusado";if(event=="reject"){o["status"]="Cancelado";for(const auto& i:o["items"])db.release(i["id"],i["quantity"]);}observer.onEvent(o,"payment."+event);}else if(event=="advance"){o["status"]=DeliveryState::next(o);if(o["status"]=="Entregue"){o["deliveredAt"]=timestamp();if(o["payment"]=="delivery")o["paymentStatus"]="Aprovado";o["notification"]="Entrega concluída. Notificação registrada na caixa de saída de teste.";observer.onEvent(o,"order.delivered");}}else throw std::invalid_argument("Evento inválido.");db.update(o);tx.commit();return o;}
};
