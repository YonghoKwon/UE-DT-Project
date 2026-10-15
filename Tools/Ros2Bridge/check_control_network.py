"""WSL-only live relay authentication check; outputs HTTP codes, never credentials."""
import argparse
import http.client
import json
import os
from pathlib import Path
from control import ApiClient


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',required=True)
    p.add_argument('--relay-port',type=int,default=18099)
    p.add_argument('--api-port',type=int,default=8099)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    token=os.environ['MA0T10_AUTOMATION_TOKEN']
    checks=[]
    for name,auth in [('missing',''),('wrong','Bearer intentionally-invalid'),('valid','Bearer '+token)]:
        conn=http.client.HTTPConnection(args.host,args.relay_port,timeout=2)
        try:
            headers={'Host':f'127.0.0.1:{args.api_port}'}
            if auth:headers['Authorization']=auth
            conn.request('GET','/v1/status',headers=headers)
            response=conn.getresponse();code=response.status;response.read()
            assert code==(200 if name=='valid' else 401)
            checks.append({'case':name,'http_status':code})
        finally:conn.close()
    assert ApiClient(args.host,args.relay_port,args.api_port,token).call('status',{})['api_enabled']
    args.output.write_text(json.dumps({'ok':True,'checks':checks},indent=2),encoding='utf8')
    print('WSL relay auth checks passed: 401/401/200; valid client Host accepted.')


if __name__=='__main__':main()
