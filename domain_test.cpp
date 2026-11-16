#include "../backend/domain.hpp"
#include <iostream>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> void fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Operação inválida foi aceita");}
int main(int argc,char** argv){try{
    check(argc==2,"Informe o catálogo");SQLiteRepository db(":memory:",argv[1]);SaoPauloShipping shipping;OutboxObserver observer(db);OrderService service(db,shipping,observer);
    check(shipping.quote("01310100")["shipping"]==800,"Frete central");check(shipping.quote("05000000")["shipping"]==1200,"Frete expandido");fails([&]{shipping.quote("99999999");});fails([&]{shipping.quote("123");});
    Json input={{"name","Cliente teste"},{"email","teste@example.com"},{"cep","01310100"},{"street","Avenida de teste"},{"number","10"},{"complement",""},{"notes","Sem vela"},{"date",dateAt(1)},{"slot","09:00–12:00"},{"payment","pix"},{"items",Json::array({{{"id",1},{"quantity",2},{"price",1}}})}};
    auto key=randomHex();auto order=service.create(input,key);check(order["total"]==3780,"Servidor deve recalcular preços");check(order["notes"]=="Sem vela","Observações persistidas");check(db.products()[0]["stock"]==22,"Reserva de estoque");check(service.create(input,key)["id"]==order["id"],"Idempotência");check(db.products()[0]["stock"]==22,"Não reservar duas vezes");
    auto changed=input;changed["notes"]="Outra nota";fails([&]{service.create(changed,key);});fails([&]{service.authorized(order["id"],"Bearer errado");});
    const auto bearer="Bearer "+order["token"].get<std::string>();fails([&]{service.simulate(order["id"],bearer,"advance");});service.simulate(order["id"],bearer,"approve");fails([&]{service.simulate(order["id"],bearer,"reject");});for(int i=0;i<3;i++)order=service.simulate(order["id"],bearer,"advance");check(order["status"]=="Entregue"&&order.contains("deliveredAt"),"Entrega registrada");fails([&]{service.simulate(order["id"],bearer,"advance");});
    auto invalid=input;invalid["items"]=Json::array({{{"id",1},{"quantity",1}},{{"id",8},{"quantity",1}}});fails([&]{service.create(invalid,randomHex());});check(db.products()[0]["stock"]==22,"Rollback de todos os itens");
    invalid=input;invalid["date"]="2020-01-01";fails([&]{service.create(invalid,randomHex());});invalid=input;invalid["slot"]="12:00–15:00";fails([&]{service.create(invalid,randomHex());});invalid=input;invalid["items"][0]["quantity"]=-1;fails([&]{service.create(invalid,randomHex());});invalid=input;invalid["notes"]=std::string(301,'x');fails([&]{service.create(invalid,randomHex());});
    auto declined=service.create(input,randomHex());auto token="Bearer "+declined["token"].get<std::string>();service.simulate(declined["id"],token,"reject");check(db.products()[0]["stock"]==22,"Recusa devolve estoque");fails([&]{service.simulate(declined["id"],token,"reject");});check(db.products()[0]["stock"]==22,"Devolução única");
    std::cout<<"Todos os testes de domínio passaram.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FALHA: "<<e.what()<<'\n';return 1;}}
