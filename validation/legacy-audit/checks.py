import numpy as np, math
mu0=4e-7*math.pi
def loopB(P, c, axis, a, I, n=4000):
    # numeric Biot-Savart of a circular loop; P (M,3)
    t=np.linspace(0,2*np.pi,n,endpoint=False); axis=np.array(axis,float)
    u=np.array([1.,0,0]) if abs(axis[0])<0.9 else np.array([0,1.,0]); e1=np.cross(axis,u); e1/=np.linalg.norm(e1); e2=np.cross(axis,e1)
    pts=np.array(c)+a*(np.outer(np.cos(t),e1)+np.outer(np.sin(t),e2)); dl=a*(2*np.pi/n)*(-np.outer(np.sin(t),e1)+np.outer(np.cos(t),e2))
    B=np.zeros((len(P),3))
    for i in range(n):
        r=P-pts[i]; rn=np.linalg.norm(r,axis=1)[:,None]
        B+=np.cross(dl[i],r)/rn**3
    return mu0*I/(4*np.pi)*B
R=0.2; N=312; I=1.5; g=42.577e6
Bc=(4/5)**1.5*mu0*N*I/R
print("thin-wire Helmholtz centre, N=312, 1.5 A: %.5f mT -> %.1f Hz; design target 89400/g = %.5f mT; N exact for target = %.2f"%(Bc*1e3,g*Bc,89400/g*1e3,89400/g*R/((4/5)**1.5*mu0*I)))
pair=lambda P: loopB(P,[0,-R/2,0],[0,1,0],R,N*I)+loopB(P,[0,R/2,0],[0,1,0],R,N*I)
B0=pair(np.zeros((1,3)))[0,1]
for s in [0.01,0.02,0.03]:
    ax=pair(np.array([[0,s,0]]))[0,1]/B0-1; rad=pair(np.array([[s,0,0]]))[0,1]/B0-1
    print("s=%.3f axial err %.4e (4th: %.4e)  radial err %.4e (4th: %.4e)"%(s,ax,-1.152*(s/R)**4,rad,-1.152*0.375*(s/R)**4))
# finite solenoid on axis vs numeric sum of loops (approx model geometry)
L=0.1;a=0.02045;Nt=400
xs=np.linspace(-L/2+L/Nt/2,L/2-L/Nt/2,Nt)
P=np.array([[0,0,0],[0.04,0,0],[-0.055,0,0]],float)
Bn=sum(loopB(P,[x,0,0],[1,0,0],a,1.0,1000) for x in xs)[:,0]
prof=lambda x:0.5*((x+L/2)/math.hypot(x+L/2,a)-(x-L/2)/math.hypot(x-L/2,a))
for i,x in enumerate(P[:,0]): print("solenoid x=%.3f numeric %.5e analytic %.5e T/A"%(x,Bn[i],mu0*Nt/L*prof(x)))
# exact 2-layer coil, centre
t1=[(-0.05+0.000225+0.00045*k,0.020225) for k in range(222)]+[(-0.05+0.000225+0.00045*k,0.020675) for k in range(178)]
Bx=sum(loopB(np.zeros((1,3)),[x,0,0],[1,0,0],r,1.0,1000) for x,r in t1)[0,0]
iTx=7.95/math.hypot(6.7,2*math.pi*89400*2.53e-3)
print("2-layer centre %.6e T/A (map centre %.6e); iTx %.5f mA; t90 %.2f us; long-solenoid t90 %.2f us"%(Bx,0.005029625044057467,iTx*1e3,1/(4*g*0.5*Bx*iTx)*1e6,1/(4*g*0.5*mu0*400/0.1*iTx)*1e6))
# Curie
hbar=1.054571817e-34;kB=1.380649e-23;gam=2*math.pi*g
M0=6.69e28*gam**2*hbar**2*(89400/g)/(4*kB*293)
print("M0 at 2.0997 mT, 293 K: %.4e A/m; Nh water check %.4e"%(M0, 2*55.51e3*6.02214e23/1e3*0.998))
