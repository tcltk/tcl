const N=1000000;
function best(f,a){let b=1e18,r;for(let i=0;i<5;i++){const t=process.hrtime.bigint();r=f(a);const d=Number(process.hrtime.bigint()-t)/1e6;if(d<b)b=d;}return [b,r];}
function c(){return new Uint8Array(N).fill(1);}
function k(s0){const s=s0.slice();s[0]=s[1]=0;for(let i=2;i*i<N;i++)if(s[i])for(let j=i*i;j<N;j+=i)s[j]=0;return s;}
function n(s){let c=0;for(let i=0;i<s.length;i++)c+=s[i];return c;}
function c2(){return new Array(N).fill(1);}
for (const [nom,C] of [["JavaScript (Node) Uint8Array",c],["JavaScript (Node) Array",c2]]){
let [tc,s]=best(C);let [tk,s2]=best(k,s);let [tn,r]=best(n,s2);
console.log(nom.padEnd(32),tc.toFixed(1).padStart(7),tk.toFixed(1).padStart(7),tn.toFixed(1).padStart(7),(tc+tk+tn).toFixed(1).padStart(7),"->",r);}
