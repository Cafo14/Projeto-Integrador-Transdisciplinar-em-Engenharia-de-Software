# API HTTP — versão sandbox

Mesma origem do frontend. JSON UTF-8. Máximo de corpo: 32 KiB. O serviço não disponibiliza CORS irrestrito. Não há listagem pública de pedidos.

| Método | Rota | Resultado |
|---|---|---|
| GET | `/api/health` | `{ok:true, mode:"sandbox"}` |
| GET | `/api/products` | Catálogo com preço em centavos e estoque atual |
| GET | `/api/shipping?cep=01310100` | Frete em centavos e estimativa |
| GET | `/api/slots?date=AAAA-MM-DD` | Horários e disponibilidade |
| POST | `/api/orders` | Cria pedido com estoque reservado; header `Idempotency-Key` obrigatório |
| GET | `/api/orders/:id` | Pedido; header `Authorization: Bearer TOKEN` obrigatório |
| POST | `/api/orders/:id/simulate` | Evento de teste; mesmo token obrigatório |

Criação: cabeçalho de idempotência com 32–80 caracteres alfanuméricos/hífens, gerado aleatoriamente. Repetir a mesma chave e o mesmo corpo retorna o mesmo pedido; reutilizar a chave com outro corpo é rejeitado. Guarde a chave até obter uma resposta definitiva.

```json
{
  "name": "Cliente de teste",
  "email": "teste@example.com",
  "cep": "01310100",
  "street": "Rua de teste",
  "number": "10",
  "complement": "",
  "date": "DATA_DISPONIVEL_EM_AAAA-MM-DD",
  "slot": "09:00–12:00",
  "notes": "Sem vela",
  "payment": "pix",
  "items": [{"id": 1, "quantity": 2}]
}
```

`payment`: `pix`, `credit`, `debit`, `delivery`. Preços, frete, estoque, total, ID e status são definidos pelo servidor. IDs repetidos são agregados. De 1 a 99 unidades por produto. A data deve estar entre amanhã e 14 dias em UTC−03:00 (São Paulo); a capacidade é validada dentro da transação.

Resposta inclui `id`, `token`, `createdAt`, `items`, `shipping`, `total`, `paymentStatus`, `status`, `notification` e dados de entrega. O token é um segredo de acesso ao pedido; não deve ser colocado em URLs ou logs. Como não há contas nesta versão, a chave de idempotência também funciona como segredo de recuperação da criação.

Simulação recebe `{"event":"approve"}`, `{"event":"reject"}` ou `{"event":"advance"}`. Pagamentos processados não podem ser reprocessados; pedido recusado não avança e tem o estoque devolvido uma única vez. Pagamento na entrega é marcado aprovado apenas ao concluir o fluxo de teste da entrega.

Erros: 400 para validação/JSON; 404 para pedido inexistente ou token incorreto; 500 para falhas internas. O domínio não aceita valores ou estados de pagamento enviados livremente pelo cliente.
