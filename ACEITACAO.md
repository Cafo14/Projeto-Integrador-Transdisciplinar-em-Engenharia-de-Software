# Rastreabilidade das histórias

“Implementado” refere-se ao comportamento local/sandbox, não a homologação de operação comercial. Fotos são ilustrativas. Credenciais externas não foram fornecidas.

| História | Cobertura | Pendência |
|---|---|---|
| US01 | Grade, fotos, nome, descrição, preço, responsividade e indisponibilidade | Fotos reais por produto |
| US02 | Filtros combinados por sabor/tamanho sem reload | — |
| US03 | Foto ampliada, ingredientes, alergênicos, preço unitário | Revisão dos ingredientes pelo negócio |
| US04 | Busca instantânea por nome, estado vazio | — |
| US05 | Quantidade, total e confirmação visual | — |
| US06 | Incremento, decremento e remoção | — |
| US07 | Observação limitada a 300 caracteres e persistida | — |
| US08 | Resumo, endereço, frete e confirmação explícita | — |
| US09 | ID único, sucesso e evento na outbox | Envio real de e-mail/WhatsApp |
| US10 | PIX, crédito, débito, entrega | Gateway real |
| US11 | QR/código explicitamente de demonstração; eventos de teste | PIX pagável e confirmação por webhook |
| US12 | Fluxo de aprovação/recusa sem coleta insegura de cartão | Checkout tokenizado, validação pelo provedor e parcelamento |
| US13 | Pendente, Aprovado, Recusado, consulta periódica | Reconciliação real |
| US14 | CEP, rua, número, complemento, validação e endereços locais | Conta e sincronização entre dispositivos |
| US15 | Frete e estimativa por CEP, total atualizado | Tarifas/área reais |
| US16 | Datas, janelas, indisponibilidade, capacidade e persistência | Agenda operacional |
| US17 | Status, previsão e polling de 10 segundos | Eventos reais da operação/logística |
| US18 | Entregue, timestamp e evento na outbox | Notificação real de entrega |

As histórias com integração pendente **não atendem integralmente à definição de pronto de produção**. Os testes cobrem a implementação de referência entregue; esta versão não promete disponibilidade, desempenho ou segurança de uma operação comercial.
