// independent checks of physics.js conventions: Rodrigues rotation vs RK4 Bloch ODE in rotating frame; Hahn echo with the same rot
const G=2*Math.PI*42.577e6;
function rot(v,n,th){const c=Math.cos(th),s=Math.sin(th),d=n[0]*v[0]+n[1]*v[1]+n[2]*v[2];
 return [v[0]*c+(n[1]*v[2]-n[2]*v[1])*s+n[0]*d*(1-c), v[1]*c+(n[2]*v[0]-n[0]*v[2])*s+n[1]*d*(1-c), v[2]*c+(n[0]*v[1]-n[1]*v[0])*s+n[2]*d*(1-c)];}
// dM/dt = gamma M x B_eff, B_eff=(B1x,B1y, dw/gamma)
function rk4(m,w1x,w1y,dw,T,steps=20000){const h=T/steps;const f=m=>{const b=[w1x,w1y,dw];return [m[1]*b[2]-m[2]*b[1], m[2]*b[0]-m[0]*b[2], m[0]*b[1]-m[1]*b[0]];};
 for(let i=0;i<steps;i++){const k1=f(m),k2=f(m.map((x,j)=>x+h/2*k1[j])),k3=f(m.map((x,j)=>x+h/2*k2[j])),k4=f(m.map((x,j)=>x+h*k3[j]));m=m.map((x,j)=>x+h/6*(k1[j]+2*k2[j]+2*k3[j]+k4[j]));}return m;}
const w1=G*14.068e-6, tau=417.38e-6;
for(const [dw,phi] of [[0,0],[2*Math.PI*160,0],[2*Math.PI*-300,0.4]]){
 const W=Math.hypot(w1,dw), n=[-w1*Math.cos(phi)/W,-w1*Math.sin(phi)/W,-dw/W];
 const a=rot([0,0,1],n,W*tau), b=rk4([0,0,1],w1*Math.cos(phi),w1*Math.sin(phi),dw,tau);
 console.log("dw/2pi",(dw/2/Math.PI).toFixed(0),"phi",phi,"rodrigues",a.map(x=>x.toFixed(5)).join(","),"rk4",b.map(x=>x.toFixed(5)).join(","));
}
// free precession sign: physics.js applies e^{-i dw t} to mx+i my. ODE with b=(0,0,dw): dmx/dt = my*dw, dmy/dt=-mx*dw -> mx+i my ~ e^{-i dw t}. consistent.
const fp=rk4([1,0,0],0,0,2*Math.PI*100,1e-3,2000); console.log("free prec 100 Hz 1 ms: rk4",fp.map(x=>x.toFixed(4)).join(","),"expected e^{-i 0.2pi}",Math.cos(-0.2*Math.PI).toFixed(4),Math.sin(-0.2*Math.PI).toFixed(4));
// Hahn echo: 90x - tau - 180y - tau, offsets spread over 300 Hz, hard pulses (w1 large) and the real w1
for (const W1 of [G*1e-3, w1]) {
 let sx=0,sy=0,sx0=0,sy0=0,nv=2001; const te=20e-3;
 for(let i=0;i<nv;i++){const dw=2*Math.PI*(-150+300*i/(nv-1));
  const p=(m,ph,t)=>{const W=Math.hypot(W1,dw);return rot(m,[-W1*Math.cos(ph)/W,-W1*Math.sin(ph)/W,-dw/W],W*t);};
  const fr=(m,t)=>{const c=Math.cos(-dw*t),s=Math.sin(-dw*t);return [m[0]*c-m[1]*s,m[0]*s+m[1]*c,m[2]];};
  const t90=Math.PI/2/W1; let m=p([0,0,1],0,t90); sx0+=m[0]; sy0+=m[1]; m=fr(m,te); m=p(m,Math.PI/2,2*t90); m=fr(m,te); sx+=m[0]; sy+=m[1];}
 console.log("Hahn echo, B1rot",(W1/G*1e6).toFixed(1),"uT: |M+| right after 90 =",(Math.hypot(sx0,sy0)/nv).toFixed(4)," at echo =",(Math.hypot(sx,sy)/nv).toFixed(4), "phase",Math.atan2(sy,sx).toFixed(3));
}
