#include "vendor/httplib.h"
#include "domain.hpp"
#include <iostream>
#include <cstdlib>

Json publicOrder(Json order){order.erase("request");return order;}
int main(int argc,char** argv){
    try {
        const std::string root=argc>1?argv[1]:"dist";
        const std::string database=argc>2?argv[2]:"data/shop.sqlite";
        const int port=argc>3?std::stoi(argv[3]):8080;
        const char* mode=std::getenv("APP_MODE");if(mode&&std::string(mode)!="sandbox")throw std::runtime_error("Somente APP_MODE=sandbox está implementado. Configure adaptadores reais antes de produção.");
        SQLiteRepository repository(database,root+"/products.json");SaoPauloShipping shipping;OutboxObserver notifications(repository);OrderService service(repository,shipping,notifications);httplib::Server server;
        server.set_payload_max_length(32*1024);server.set_read_timeout(5,0);server.set_write_timeout(5,0);
        server.set_default_headers({{"X-Content-Type-Options","nosniff"},{"Referrer-Policy","same-origin"},{"Cache-Control","no-store"},{"Content-Security-Policy","default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src https://fonts.gstatic.com; script-src 'self'; connect-src 'self'; frame-ancestors 'none'"}});
        auto guarded=[&](auto handler){return [&,handler](const httplib::Request& req,httplib::Response& res){try{std::lock_guard<std::mutex> lock(repository.mutex);res.set_content(handler(req).dump(),"application/json; charset=utf-8");}catch(const std::out_of_range& e){res.status=404;res.set_content(Json{{"error",e.what()}}.dump(),"application/json");}catch(const Json::exception&){res.status=400;res.set_content("{\"error\":\"JSON ou campos inválidos.\"}","application/json");}catch(const std::invalid_argument& e){res.status=400;res.set_content(Json{{"error",e.what()}}.dump(),"application/json");}catch(const std::exception& e){std::cerr<<"Falha interna: "<<e.what()<<'\n';res.status=500;res.set_content("{\"error\":\"Falha interna ao processar a solicitação.\"}","application/json");}};};
        server.Get("/api/health",guarded([](const auto&){return Json{{"ok",true},{"mode","sandbox"}};}));
        server.set_pre_routing_handler([](const httplib::Request& r,httplib::Response& res){if(r.path=="/config.json"){res.set_content("{\"mode\":\"api\"}","application/json");return httplib::Server::HandlerResponse::Handled;}return httplib::Server::HandlerResponse::Unhandled;});
        server.Get("/api/products",guarded([&](const auto&){return repository.products();}));
        server.Get("/api/shipping",guarded([&](const auto& r){return shipping.quote(r.get_param_value("cep"));}));
        server.Get("/api/slots",guarded([&](const auto& r){return service.slots(r.get_param_value("date"));}));
        server.Post("/api/orders",guarded([&](const auto& r){return publicOrder(service.create(Json::parse(r.body),r.get_header_value("Idempotency-Key")));}));
        server.Get(R"(/api/orders/(DA-[a-f0-9]+))",guarded([&](const auto& r){return publicOrder(service.authorized(r.matches[1].str(),r.get_header_value("Authorization")));}));
        server.Post(R"(/api/orders/(DA-[a-f0-9]+)/simulate)",guarded([&](const auto& r){return publicOrder(service.simulate(r.matches[1].str(),r.get_header_value("Authorization"),field(Json::parse(r.body),"event",20)));}));
        if(!server.set_mount_point("/",root))throw std::runtime_error("Diretório do frontend não encontrado.");
        std::cout<<"Doce Ateliê · sandbox · http://127.0.0.1:"<<port<<std::endl;
        if(!server.listen("127.0.0.1",port))throw std::runtime_error("Não foi possível abrir a porta HTTP.");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
