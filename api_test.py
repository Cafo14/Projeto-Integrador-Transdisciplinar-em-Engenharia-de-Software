"""Execute somente contra uma base descartável de sandbox."""
import json, sys, uuid, urllib.request, urllib.error
from datetime import datetime, timedelta, timezone
BASE = sys.argv[1] if len(sys.argv)>1 else 'http://127.0.0.1:8080'
def call(path, body=None, headers=None, status=200):
    req=urllib.request.Request(BASE+path, data=json.dumps(body).encode() if body is not None else None, headers={'Content-Type':'application/json', **(headers or {})})
    try:
        with urllib.request.urlopen(req, timeout=5) as r: actual=r.status; result=json.load(r)
    except urllib.error.HTTPError as e: actual=e.code; result=json.load(e)
    assert actual==status,(path,actual,status,result)
    return result
assert call('/api/health')['mode']=='sandbox'
assert call('/config.json')['mode']=='api'
products=call('/api/products'); before=products[0]['stock']
assert call('/api/shipping?cep=01310100')['shipping']==800
call('/api/shipping?cep=99999999',status=400)
date=(datetime.now(timezone(timedelta(hours=-3)))+timedelta(days=1)).date().isoformat()
slots=call('/api/slots?date='+date)
assert slots[0]['available'] and not slots[1]['available']
body=dict(name='Cliente teste HTTP', email='teste@example.com',cep='01310100',street='Rua de teste',number='10',complement='',notes='Teste de integração',date=date,slot=slots[0]['time'],payment='credit',items=[dict(id=1,quantity=2,price=1)],total=1)
headers={'Idempotency-Key':str(uuid.uuid4())}
order=call('/api/orders',body,headers)
assert order['total']==products[0]['price']*2+800
assert order['notes']==body['notes']
assert call('/api/orders',body,headers)['id']==order['id']
assert call('/api/products')[0]['stock']==before-2
call('/api/orders',{**body,'notes':'Mudou'},headers,status=400)
call('/api/orders/'+order['id'],status=404)
auth={'Authorization':'Bearer '+order['token']}
assert call('/api/orders/'+order['id'],headers=auth)['id']==order['id']
path='/api/orders/'+order['id']+'/simulate'
call(path,{'event':'advance'},auth,status=400)
assert call(path,{'event':'approve'},auth)['paymentStatus']=='Aprovado'
call(path,{'event':'reject'},auth,status=400)
for expected in ['Em preparo','Saiu para entrega','Entregue']:
    delivered=call(path,{'event':'advance'},auth);assert delivered['status']==expected
assert delivered['deliveredAt']
call(path,{'event':'advance'},auth,status=400)
for bad in [-1,0,100,1.5]:
    call('/api/orders',{**body,'items':[dict(id=1,quantity=bad)]},{'Idempotency-Key':str(uuid.uuid4())},status=400)
print('PASS: health, config, catálogo, frete, horários, total autoritativo, idempotência, estoque, autorização, validação e entrega.')
