# Validação — 13/11/2026

- `make test`: compilação C++17 sem warnings; testes de domínio passaram.
- `python3 tests/api_test.py http://127.0.0.1:8080`: passou contra SQLite descartável. Validou health, configuração API, catálogo, frete, horários, preços recalculados, criação idempotente, estoque, token obrigatório, quantidades inválidas, pagamento e entrega.
- `node --check dist/app.js`: sintaxe JavaScript válida.
- Servidor estático: HTTP 200 para a página inicial.
- Navegador: filtro Chocolate + pesquisa inexistente retornou estado vazio; limpeza restaurou oito produtos; modal exibiu ingredientes e alergênicos; inclusão de duas unidades de Chocolate Belga resultou em R$ 29,80.
- Checkout: CEP 01310-100 gerou R$ 8,00 de frete; janela 12–15 ficou desabilitada; pedido para 14/11/2026 09–12 exibiu total de R$ 37,80, endereço e observação; confirmação criou ID e QR explicitamente de teste.
- Pagamento e entrega no navegador: Pendente → Aprovado; Recebido → Em preparo → Saiu para entrega → Entregue, com timestamp e notificação simulada.
- Responsividade: viewport 390 × 844, documento e scrollWidth de 390, sem overflow horizontal na vitrine.

Correções encontradas pelos testes: acesso ao campo `slot` via `form.elements` para evitar colisão DOM; configuração API servida antes do arquivo estático para evitar seleção indevida do modo demonstração.

Limites: navegador integrado, sem matriz de Chrome/Firefox/Safari ou leitor de tela. Não realizados testes de carga, de gateway real, envio real de notificações ou entregas reais. Ver `ACEITACAO.md` para critérios pendentes.
