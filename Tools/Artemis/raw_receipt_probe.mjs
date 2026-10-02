// Local fault-injection broker: accepts SEND bytes and deliberately withholds receipts.
import net from 'node:net';
import fs from 'node:fs';
const port = Number(process.argv[2] || 18617);
const output = process.argv[3];
const report = {connections:0, sends:0, bytes:0, requests:[], errors:[]};
function save() { if(output) fs.writeFileSync(output,JSON.stringify(report,null,2)); }
const server = net.createServer(socket => {
  report.connections++; let pending=Buffer.alloc(0);
  socket.on('error',error=>{report.errors.push(error.message);save();});
  socket.on('data',data=>{
    pending=Buffer.concat([pending,data]);
    while(pending.length){
      while(pending[0]===10||pending[0]===13)pending=pending.subarray(1);
      const end=pending.indexOf('\n\n'); if(end<0)return;
      const lines=pending.subarray(0,end).toString('utf8').split('\n');
      const command=lines.shift();const headers={};
      for(const line of lines){const colon=line.indexOf(':');if(colon>=0)headers[line.slice(0,colon)]=line.slice(colon+1);}
      const bytes=Number(headers['content-length']||0);const total=end+2+bytes+1;
      if(pending.length<total)return;
      if(command==='CONNECT'||command==='STOMP')socket.write('CONNECTED\nversion:1.2\nheart-beat:0,0\n\n\0');
      if(command==='SEND'){report.sends++;report.bytes+=bytes;report.requests.push(headers.receipt);save();}
      pending=pending.subarray(total);
    }
  });
});
server.listen(port,'127.0.0.1',()=>{save();process.stdout.write(`receipt probe listening ${port}\n`);});
process.on('SIGTERM',()=>{save();server.close();});
