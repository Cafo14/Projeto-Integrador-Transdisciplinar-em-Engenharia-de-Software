# Doce Ateliê — loja de cupcakes

Aplicação de referência com **HTML, CSS e JavaScript sem framework**, API **C++17** e **SQLite**. Interface em português, responsiva, com valores em centavos e apresentação em reais.

## Executar a aplicação completa

Requisitos: compilador C++17, Make, cabeçalhos e biblioteca SQLite3. As bibliotecas HTTP e JSON estão incluídas em `backend/vendor`.

```sh
cd /Users/carlosaraujo/Documents/Codex/cupcake-shop
make test
make run
```

Abra http://127.0.0.1:8080. O servidor entrega a interface e `/config.json` em modo API automaticamente; não é necessário modificar o front-end. O SQLite é criado em `data/shop.sqlite`, ignorado pelo Git. Ao reiniciar, pedidos, estoque e eventos de notificação permanecem salvos.

Alternativa com CMake:

```sh
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
mkdir -p data
./build-cmake/cupcake-server dist data/shop.sqlite 8080
```

Somente a demonstração visual, sem C++:

```sh
python3 -m http.server 4173 --directory dist
```

Nesse caso, abra http://127.0.0.1:4173. A versão publicada no Sites é essa demonstração estática: pedidos, endereços e estoque simulado ficam no navegador, sem sincronização entre dispositivos. O Sites não executa o binário C++.

## O que está implementado

- Catálogo de oito produtos, fotos ilustrativas, descrição, preço e disponibilidade.
- Filtros por sabor e tamanho, busca instantânea e estado sem resultados.
- Modal com foto ampliada, ingredientes, alergênicos e quantidade.
- Carrinho persistido no navegador, atualização de quantidades, remoção e totais.
- Checkout com dados obrigatórios, múltiplos endereços locais, notas de até 300 caracteres, resumo e confirmação explícita.
- Frete de exemplo por faixa de CEP: 01–02 → R$ 8; 03–05 → R$ 12; outras faixas não atendidas.
- Entrega entre amanhã e os próximos 14 dias, calculada em America/Sao_Paulo; janelas 09–12 e 15–18, limite de dez pedidos por janela. A janela 12–15 está indisponível nesta configuração de exemplo.
- Número de pedido aleatório, idempotência, reserva transacional de estoque e devolução única na recusa do pagamento.
- Escolha PIX, crédito, débito ou pagamento na entrega; status Pendente, Aprovado e Recusado.
- Rastreamento com atualização a cada dez segundos no modo API; etapas Recebido, Em preparo, Saiu para entrega, Entregue e registro de data/hora.
- Caixa de saída transacional (`outbox`) para confirmações de pedido e entrega.

## Limite importante: sandbox, não operação comercial

**Não há cobrança real, PIX pagável, captura de cartão, envio de e-mail/WhatsApp ou despacho real.** O QR Code contém um identificador de demonstração, explicitamente sinalizado na interface. Aprovação, recusa e movimentação de entrega são eventos acionados por botões de teste. Nenhum PAN/CVV é solicitado ou armazenado. Não há parcelamento real.

`APP_MODE` aceita apenas `sandbox`: o servidor recusa iniciar em outro modo. Isso evita apresentar os adaptadores simulados como integrações prontas. Os endpoints de simulação só fazem sentido em ambiente local de desenvolvimento.

Para operar comercialmente, ainda é necessário implementar e homologar:

1. Adaptador de gateway de pagamento para PIX dinâmico e checkout hospedado/tokenização de cartões, com credenciais de ambiente, webhooks assinados, conferência de valor/moeda/pedido e deduplicação de eventos.
2. Expiração das reservas de pagamentos pendentes, cancelamento/reembolso e reconciliação do provedor.
3. Consumidor da outbox com serviço de e-mail ou WhatsApp; tentativas, chave de deduplicação e estado de entrega da mensagem.
4. Integração logística ou painel operacional autenticado para alterar etapas, horários e estoque. Os botões de simulação devem ser removidos.
5. HTTPS, autenticação de clientes e administradores, autorização, limite de requisições, backup, política de retenção e proteção de dados. Nesta referência os pedidos usam um token aleatório de acesso guardado neste navegador; endereços são locais e não há recuperação de conta.
6. Cadastro real de produtos, fotos correspondentes, ingredientes/alergênicos revisados e área/tarifa de entrega do negócio. Fotos atuais são ilustrativas e reutilizadas.

O servidor escuta apenas em `127.0.0.1`. Não o exponha como serviço comercial sem concluir os itens acima. O SQLite armazena os dados do pedido em JSON e mantém produtos e eventos em tabelas separadas; clientes/endereço/pagamento/entrega são snapshots do pedido, não cadastros relacionais independentes.

## Padrões e organização

| Padrão | Uso | Arquivo |
|---|---|---|
| Repository | Interface de persistência e implementação SQLite | `backend/repository.hpp` |
| Strategy | Frete por região e meio de pagamento | `backend/domain.hpp` |
| Factory | Criação da estratégia de pagamento | `backend/domain.hpp` |
| Observer | Eventos enviados à outbox | `backend/domain.hpp` |
| Máquina de estados | Transições permitidas de entrega | `backend/domain.hpp` |
| RAII / Unit of Work | Commit ou rollback de estoque, pedido e notificação | `backend/repository.hpp` |
| Service Layer | Validação e orquestração de pedidos | `backend/domain.hpp` |

As estratégias de cartão/PIX compartilham um único adaptador sandbox. `DeliveryState` é uma máquina de estados explícita, não uma hierarquia de classes do padrão GoF State.

```text
dist/                 Interface e catálogo inicial
backend/main.cpp      Rotas HTTP e inicialização
backend/domain.hpp    Serviços, validações e padrões
backend/repository.hpp SQLite, statements preparados e transações
tests/                Testes de domínio e integração HTTP
docs/                 API, critérios de aceitação e evidências
```

## Testes

`make test` compila e executa testes de frete, preços autoritativos, idempotência, autorização por token, estoque, rollback, datas/horários, quantidade inválida, limite de observações, recusa e entrega. Os testes usam SQLite em memória.

Com o servidor rodando em uma base **descartável**, execute `python3 tests/api_test.py http://127.0.0.1:8080`. Esse teste cria pedidos de teste e verifica rotas, persistência, estoque e transições. Evidências desta execução em `docs/VALIDACAO.md`.

## Bibliotecas e imagens

- cpp-httplib v0.18.3 — MIT, copyright Yuji Hirose; licença no cabeçalho incluído.
- nlohmann/json v3.11.3 — MIT, copyright Niels Lohmann; licença no cabeçalho incluído.
- qrcode-generator v1.4.4 — MIT, copyright Kazuhiko Arase; licença no JavaScript incluído.
- Fotos ilustrativas: Unsplash, referências em `docs/ASSETS.md`.
- Fontes DM Sans e Playfair Display via Google Fonts; fontes locais do sistema são usadas na indisponibilidade do serviço.
