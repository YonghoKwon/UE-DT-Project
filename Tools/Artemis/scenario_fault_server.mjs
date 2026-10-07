// Isolated STOMP fault fixture; never connects to or changes the real Artemis broker.
import net from 'node:net';
import fs from 'node:fs';
const args=Object.fromEntries(process.argv.slice(2).reduce((a,x,i,v)=>i%2?a:[...a,[x.replace(/^--/,''),v[i+1]]],[]));
const mode=args.mode??'close',port=Number(args.port??18679);
const report={mode,port,connections:0,sends:0,bytes:0,receipts:0};
const save=()=>{if(args.report)fs.writeFileSync(args.report,JSON.stringify(report,null,2));};
const server=net.createServer(socket=>{
  report.connections++;save();let pending=Buffer.alloc(0),closed=false;
  socket.on('error',()=>{});socket.on('close',()=>{closed=true;});
  socket.on('data',chunk=>{
    pending=Buffer.concat([pending,chunk]);
    while(pending.length){
      while(pending.length&&(pending[0]===10||pending[0]===13))pending=pending.subarray(1);
      let boundary=pending.indexOf('\n\n'),separator=2;
      if(boundary<0){boundary=pending.indexOf('\r\n\r\n');separator=4;}
      if(boundary<0)return;
      const lines=pending.subarray(0,boundary).toString('utf8').split(/\r?\n/),command=lines.shift();
      const headers=Object.fromEntries(lines.map(x=>{const i=x.indexOf(':');return [x.slice(0,i),x.slice(i+1)];}));
      const bodyStart=boundary+separator;
      const bodyEnd=headers['content-length']!==undefined?bodyStart+Number(headers['content-length']):pending.indexOf(0,bodyStart);
      if(bodyEnd<0||pending.length<=bodyEnd)return;
      if(pending[bodyEnd]!==0){socket.destroy();return;}
      pending=pending.subarray(bodyEnd+1);
      if(command==='CONNECT'||command==='STOMP'){
        if(mode==='auth'){socket.end('ERROR\nmessage:fixture authentication rejection\n\nfixture\0');return;}
        const connected=()=>{if(!closed)socket.write('CONNECTED\nversion:1.2\nheart-beat:0,0\n\n\0');};
        if(mode==='delay')setTimeout(connected,Number(args['delay-ms']??1500));else connected();
      }else if(command==='SEND'){
        report.sends++;report.bytes+=bodyEnd-bodyStart;save();
        if(mode==='close'){socket.destroy();return;}
        if(mode==='no_receipt'||mode==='overload')continue;
        if(headers.receipt){socket.write(`RECEIPT\nreceipt-id:${headers.receipt}\n\n\0`);report.receipts++;save();}
      }else if(headers.receipt)socket.write(`RECEIPT\nreceipt-id:${headers.receipt}\n\n\0`);
    }
  });
});
server.listen(port,'127.0.0.1',()=>{save();console.log(`fixture ready ${port} ${mode}`);});
const stop=()=>{save();server.close();process.exit(0);};process.on('SIGINT',stop);process.on('SIGTERM',stop);
