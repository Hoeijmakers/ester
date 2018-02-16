// test version for inclusion of the entropy diffusion

#include "ester-config.h"
#include "star.h"
#include <stdlib.h>
#include <string.h>
#include "symbolic.h"

#include "matplotlib.h"

void star1d::fill() {
	Y0=1.-X0-Z0;
        //printf("begin fill call init_comp\n");
	init_comp();

    eq_state();
    opacity();
    nuclear();

    m=4*PI*(map.gl.I,rho*r*r)(0);
    R=pow(M/m/rhoc,1./3.);

    pi_c=(4*PI*GRAV*rhoc*rhoc*R*R)/pc;
    Lambda=rhoc*R*R/Tc;

    calc_units();

    atmosphere();

    phiex=phi(-1)/rex;
    w=zeros(nr,1);G=zeros(nr,1);vr=zeros(nr,1);vt=zeros(nr,1);

    Omega=0;Omega_bk=0;Ekman=0;Omegac=0;

// for the output
        schwarz=-(D,p)*((D,log(T))-eos.del_ad*(D,log(p)));
	schwarz = schwarz/r/r;
	schwarz.setrow(0, zeros(1, nth));
        //printf("size of schwarz %d,%d\n",schwarz.nrows(),schwarz.ncols());


}

solver *star1d::init_solver(int nvar_add) {
	int nvar;
	solver *op;
	
	nvar=31; // total number of variables
                 // variables added (lnXh, Wr, xi_eff, xi_turb)
	
	op=new solver();
	op->init(ndomains,nvar+nvar_add,"full");
	
	op->maxit_ref=10;op->use_cgs=0;op->maxit_cgs=20;
	op->rel_tol=1e-12;op->abs_tol=1e-20;
	register_variables(op);
	
	return op;
}

void star1d::register_variables(solver *op) {
	int i,var_nr[ndomains];
	
	for(i=0;i<ndomains;i++) 
		var_nr[i]=map.gl.npts[i];
	op->set_nr(var_nr);

	op->regvar("Phi");
	op->regvar("log_p");
	op->regvar_dep("p");
	op->regvar("pi_c");
	op->regvar("log_T");
	op->regvar_dep("T");
	op->regvar("Lambda");
	op->regvar("Ri");
	op->regvar("dRi");
	op->regvar_dep("r");
	op->regvar_dep("rz");
	op->regvar_dep("log_rhoc");
	op->regvar("log_pc");
	op->regvar("log_Tc");
	op->regvar("log_R");
	op->regvar("m");
	op->regvar("ps");
	op->regvar("Ts");
	op->regvar("lum");
	op->regvar("Frad");
	op->regvar_dep("rho");
	op->regvar_dep("s");
	op->regvar("Teff");
	op->regvar("gsup");
	op->regvar_dep("opa.xi");
	op->regvar_dep("opa.k");
	op->regvar_dep("nuc.eps");
// Evolution of Xh
	op->regvar("lnXh");
	op->regvar("Wr");
// Insertion turbulent diffusion
	op->regvar_dep("xi_turb");
	op->regvar_dep("xi_eff");

}

FILE *RHS;
FILE *entrop;
double star1d::solve(solver *op) {
    matrix_map error_map;
    return solve(op, error_map, 0);
}

double star1d::solve(solver *op, matrix_map& error_map, int nit) {
	int info[5];
	matrix rho_prec;
	double err,err2;
	
//	printf("**********  start of star1d::solve\n");
	//check_map();
	RHS=fopen("all_rhs.txt","a");
	entrop=fopen("entropie.txt","a");

	new_check_map();
	
	op->reset();
	if (config.verbose == 19) printf("solve : check_map done\n");
	solve_definitions(op);
	if (config.verbose == 19) printf("solve : check_map done\n");
	solve_poisson(op);
	if (config.verbose == 19) printf("solve : check_map done\n");
	solve_pressure(op);
	if (config.verbose == 19) printf("solve : check_map done\n");
	new_solve_temp(op);
	//solve_temp(op);
	if (config.verbose == 19) printf("solve : temp done\n");
	solve_atm(op);
	solve_dim(op);
	solve_map(op);
	solve_gsup(op);
	solve_Teff(op);
	if (config.verbose == 19) printf("solve : Teff done\n");
// Evolution of Xh
        solve_Xh(op);
	if (config.verbose == 19) printf("solve : Xh done\n");
        solve_Wr(op);
	if (config.verbose == 19) printf("solve : Wr done\n");
	
	op->solve(info);
	if (config.verbose == 19) printf("solve : solve done\n");
	
fclose(RHS);
// Some output verbose ----------------------
	if (config.verbose) {
		if(info[2]) {
			printf("CGS Iteration: ");
			if(info[4]>0) 
				printf("Converged after %d iterations\n",info[4]);
			else 
				printf("Not converged (Error %d)\n",info[4]);
		}
		if(info[0]) 
			printf("Solved using LU factorization\n");
		if(info[1]) {
			printf("CGS Refinement: ");
			if(info[3]>0) 
				printf("Converged after %d iterations\n",info[3]);
			else
				printf("Not converged (Error %d)\n",info[3]);
		}
	}
// End  output verbose ----------------------

	double q,h;
		
	h=1;
	q=config.newton_dmax;
	
	matrix dphi,dp,dT,dXh,dpc,dTc,dRi,dWr;
	
FILE *fic=fopen("err.txt", "a");
	dphi=op->get_var("Phi");
	err=max(abs(dphi/phi));
        error_map["Phi"](nit) = err;
  fprintf(fic,"err phi = %e\n",err);
 if (err> 1e-0) for (int k=0;k<nr;k++) fprintf(fic,"%d %e \n",k,dphi(k)/phi(k));

	dp=op->get_var("log_p");
	err2=max(abs(dp));err=err2>err?err2:err;
        error_map["log_p"](nit) = err2;
  fprintf(fic,"err P = %e\n",err2);
 if (err> 1e-0) for (int k=0;k<nr;k++) fprintf(fic,"%d %e \n",k,dp(k));
	while(exist(abs(h*dp)>q)) h/=2;

	dT=op->get_var("log_T");	
	err2=max(abs(dT));err=err2>err?err2:err;
        error_map["log_T"](nit) = err2;
  fprintf(fic,"err T = %e\n",err2);
 if (err> 1e-0) for (int k=0;k<nr;k++) fprintf(fic,"%d %e \n",k,dT(k));
	while(exist(abs(h*dT)>q)) h/=2;

// Compute dXh
	dXh=op->get_var("lnXh");	
	err2=max(abs(dXh));err=err2>err?err2:err;
  fprintf(fic,"err Xh = %e\n",err2);
 //if (err> 1e-0) for (int k=0;k<nr;k++) fprintf(fic,"%d %e \n",k,dXh(k));
	while(exist(abs(h*dXh)>q)) h/=2;
// Compute dWr
	dWr=op->get_var("Wr");	
	err2=max(abs(dWr));err=err2>err?err2:err;
  fprintf(fic,"err Wr = %e\n",err2);
 //if (err> 1e-4) for (int k=0;k<nr;k++) fprintf(fic,"%d %e %e \n",k,Wr(k),dWr(k));
	while(exist(abs(h*dWr)>q)) h/=2;
// End of dWr computation

	dpc=op->get_var("log_pc");	
	err2=fabs(dpc(0)/pc);err=err2>err?err2:err;
        error_map["log_pc"](nit) = err2;
  fprintf(fic,"err Pc = %e\n",err2);
	while(fabs(h*dpc(0))>q*pc) h/=2;

	dTc=op->get_var("log_Tc");	
	err2=fabs(dTc(0));err=err2>err?err2:err;
        error_map["log_Tc"](nit) = err2;
	while(fabs(h*dTc(0))>q) h/=2;
	
	dRi=op->get_var("Ri");	
    error_map["Ri"](nit) = max(abs(dRi));
  fprintf(fic," it = %d  h=%e\n",glit,h);
  for (int k=0;k<ndomains;k++) fprintf(fic,"dR(%d)= %e, R= %e\n",k,dRi(k),map.R(k));
	update_map(h*dRi);
	
	phi+=h*dphi;
	p+=h*dp*p;
	T+=h*dT*T;

matrix bv2=(D,p)/p/eos.G1-(D,rho)/rho;
matrix ss=entropy();
fprintf(entrop,"entropy it = %d\n",glit);
for (int k=0;k<nr;k++) fprintf(entrop,"%d %e %e\n",k,map.r(k),ss(k));
fclose(entrop);
if (details) {
FILE *fic=fopen("R.txt", "a");
fprintf(fic,"it= %d\n",glit);
for (int k=0;k<=ndomains;k++) fprintf(fic,"%d %e\n",k,map.R(k));
fclose(fic);
             }

// Evolution of Xh, dXh is the variation on ln(Xh) assumed to be small
	Xh+=h*dXh*Xh;
	Wr+=h*dWr;

	pc*=exp(h*dpc(0));
	Tc*=exp(h*dTc(0));

	err2=max(abs(dRi));err=err2>err?err2:err;
	

	rho_prec=rho;

	fill();
	
	err2=max(abs(rho-rho_prec));err=err2>err?err2:err;
	
fclose(fic);
	return err;
}

void star1d::update_map(matrix dR) {
    if(ndomains==1) return;

    double h=1,dmax=config.newton_dmax;

    matrix R0;
    R0=map.R;
    dR=dR.concatenate(zeros(1,1));
    dR(0)=0;
    while(exist(abs(h*dR)>dmax*R0)) h/=2;
    map.R+=h*dR;
    while(map.remap()) {
        h/=2;
        map.R=R0+h*dR;
    }
}

void star1d::solve_definitions(solver *op) {
	op->add_d("rho","p",rho/eos.chi_rho/p);
	op->add_d("rho","log_T",-rho*eos.d);
	op->add_d("rho","log_pc",rho/eos.chi_rho);
	op->add_d("rho","log_Tc",-rho*eos.d);
	op->add_d("rho","log_rhoc",-rho);
	
	op->add_d("r","Ri",map.J[0]);
	op->add_d("r","dRi",map.J[1]);
	op->add_d("r","Ri",map.J[2]);
	op->add_d("r","dRi",map.J[3]);
	
	op->add_d("rz","Ri",(D,map.J[0]));
	op->add_d("rz","dRi",(D,map.J[1]));
	op->add_d("rz","Ri",(D,map.J[2]));
	op->add_d("rz","dRi",(D,map.J[3]));
	
	//	Valid only for homogeneus composition !!!!!!
// MR we rescale entropy with the ideal gas constant
// changed in star2d_extra.cpp & star2d_solver
double RGP=K_BOL/UMA;
	op->add_d("s","log_T",eos.cp/RGP);
	op->add_d("s","log_Tc",eos.cp/RGP);
	op->add_d("s","log_p",-eos.cp*eos.del_ad/RGP);
	op->add_d("s","log_pc",-eos.cp*eos.del_ad/RGP);
	/*
	op->add_d("s","log_T",ones(nr,1));
	op->add_d("s","log_Tc",ones(nr,1));
	op->add_d("s","log_p",-eos.del_ad);
	op->add_d("s","log_pc",-eos.del_ad);
	*/

// Expression of \delta\khi (thermal conductivity)	
	op->add_d("opa.xi","rho",opa.dlnxi_lnrho*opa.xi/rho);
	op->add_d("opa.xi","log_rhoc",opa.dlnxi_lnrho*opa.xi);
	op->add_d("opa.xi","log_T",opa.dlnxi_lnT*opa.xi);
	op->add_d("opa.xi","log_Tc",opa.dlnxi_lnT*opa.xi);
	
// Expression of \delta\kappa (opacity) 	
	op->add_d("opa.k","log_T",3*opa.k);
	op->add_d("opa.k","log_Tc",3*opa.k);
	op->add_d("opa.k","rho",-opa.k/rho);
	op->add_d("opa.k","log_rhoc",-opa.k);
	op->add_d("opa.k","opa.xi",-opa.k/opa.xi);
	
	op->add_d("nuc.eps","rho",nuc.dlneps_lnrho*nuc.eps/rho);
	op->add_d("nuc.eps","log_rhoc",nuc.dlneps_lnrho*nuc.eps);
	op->add_d("nuc.eps","log_T",nuc.dlneps_lnT*nuc.eps);
	op->add_d("nuc.eps","log_Tc",nuc.dlneps_lnT*nuc.eps);
	
// Expression of \delta xi_turb
	matrix Pe;
	int n,j0,j1,ndom;
	
// We first compute the mask of Péclet numbers for convective/radiative domains
	Pe=zeros(nr,1);
	j0=0;
	for(n=0;n<ndomains;n++) {
		ndom=map.gl.npts[n];
		j1=j0+ndom-1;
		if (domain_type[n] == RADIATIVE) {
                   Pe.setblock(j0,j1,0,0,zeros(ndom,1));
    		} else Pe.setblock(j0,j1,0,0,Peclet*ones(ndom,1));
		j0+=ndom;
	}
	op->add_d("xi_turb","opa.xi",Pe);
	op->add_d("xi_eff","opa.xi",ones(nr,1));
	op->add_d("xi_eff","xi_turb",ones(nr,1));
}

void star1d::solve_poisson(solver *op) {
    int n,j0;
    matrix rhs;

    // printf("r(0) = %e\n", r(0));

    matrix r0 = r;
    r0(0) = 1e-10;

    op->add_l("Phi","Phi", ones(nr,1),(D,D));
    op->add_l("Phi","Phi", 2./r0, D);
    op->add_d("Phi","rho", -pi_c*ones(nr,1));
    op->add_d("Phi","pi_c", -rho);

    op->add_d("Phi","r",-2./r0/r0*(D,phi));
    op->add_d("Phi","rz",-2.*(D,D,phi)-2./r0*(D,phi));

    rhs=-(D,D,phi)-2/r0*(D,phi)+rho*pi_c;

    j0=0;
    for(n=0;n<ndomains;n++) {
        if(n==0) {
            op->bc_bot2_add_l(n,"Phi","Phi",ones(1,1),D.block(0).row(0));
            rhs(0)=-(D,phi)(0);
        } else {
            op->bc_bot2_add_l(n,"Phi","Phi",ones(1,1),D.block(n).row(0));
            op->bc_bot1_add_l(n,"Phi","Phi",-ones(1,1),D.block(n-1).row(-1));

            op->bc_bot2_add_d(n,"Phi","rz",-(D,phi).row(j0));
            op->bc_bot1_add_d(n,"Phi","rz",(D,phi).row(j0-1));

            rhs(j0)=-(D,phi)(j0)+(D,phi)(j0-1);
        }
        if(n==ndomains-1) {
            op->bc_top1_add_l(n,"Phi","Phi",ones(1,1),D.block(n).row(-1));
            op->bc_top1_add_d(n,"Phi","Phi",ones(1,1));
            op->bc_top1_add_d(n,"Phi","rz",-(D,phi).row(-1));
            rhs(-1)=-phi(-1)-(D,phi)(-1);
        } else {
            op->bc_top1_add_d(n,"Phi","Phi",ones(1,1));
            op->bc_top2_add_d(n,"Phi","Phi",-ones(1,1));
            rhs(j0+map.gl.npts[n]-1)=-phi(j0+map.gl.npts[n]-1)+phi(j0+map.gl.npts[n]);
        }
        j0+=map.gl.npts[n];
    }
    op->set_rhs("Phi",rhs);
}

void star1d::solve_pressure(solver *op) {
    int n,j0;
    matrix rhs_p,rhs_pi_c;
    char eqn[8];

    op->add_d("p","log_p",p);
    strcpy(eqn,"log_p");

    op->add_l(eqn,"p",ones(nr,1),D);
    op->add_d(eqn,"rho",(D,phi));
    op->add_l(eqn,"Phi",rho,D);

    rhs_p=-(D,p)-rho*(D,phi);
    rhs_pi_c=zeros(ndomains,1);

    j0=0;

    for(n=0;n<ndomains;n++) {
        op->bc_bot2_add_d(n,eqn,"p",ones(1,1));
        if(n>0) op->bc_bot1_add_d(n,eqn,"p",-ones(1,1));
        if(n==0) rhs_p(0)=1.-p(0);
        else rhs_p(j0)=-p(j0)+p(j0-1);
        if(n<ndomains-1) {
            op->bc_top1_add_d(n,"pi_c","pi_c",ones(1,1));
            op->bc_top2_add_d(n,"pi_c","pi_c",-ones(1,1));
        } else {
            op->bc_top1_add_d(n,"pi_c","p",ones(1,1));
            op->bc_top1_add_d(n,"pi_c","ps",-ones(1,1));
            rhs_pi_c(n)=-p(-1)+ps(0);
        }

        j0+=map.gl.npts[n];
    }
    op->set_rhs(eqn,rhs_p);
    op->set_rhs("pi_c",rhs_pi_c);
}

//Evolution Xh --------------------------------------
void star1d::solve_Xh(solver *op) {

    double Qmc2=(4*HYDROGEN_MASS-AMASS["He4"]*UMA)*C_LIGHT*C_LIGHT;
    double factor=4*HYDROGEN_MASS/Qmc2*MYR;
    double L_core=0.;
    int n,j0,ndom,nc;
    matrix the_block;

// Core parameters
//    printf("in sove_Xh conv = %d\n",conv);
    double Ed=1e-6;
    nc=0;
    for (int i=0; i<conv; i++) {
        nc += map.gl.npts[i];
    }
    matrix &rz = map.rz;  // un alias
    if (nc) { // nc = nb of grid points in core
// compute Average X in core, and total eps in core
    X_core = ((map.gl.I.block(0, 0, 0, nc-1)), (Xh*rho*r*r*rz).block(0, nc-1, 0, 0))(0);
    M_core = ((map.gl.I.block(0, 0, 0, nc-1)), (rho*r*r*rz).block(0, nc-1, 0, 0))(0);
    X_core=X_core/M_core;
    L_core = ((map.gl.I.block(0, 0, 0, nc-1)), (rho*nuc.eps*r*r*rz).block(0, nc-1, 0, 0))(0);
    }
// End core parameters


    matrix rhs;
    static symbolic S;
    static sym eq;
    static bool sym_inited = false;
    if (!sym_inited) {
// Do it only the first time the function is called
// If factor or Ed change, this should be recalculated
       sym lnX=S.regvar("lnXh");
       sym X=exp(lnX);
       sym eps=S.regvar("nuc.eps");
       sym rho=S.regvar("rho"); //new
       sym rhovr=S.regvar("Wr"); //new
       sym r=S.r;// because "r" created automatically with the symbolic object
       sym lnR=S.regvar("log_R");
       if (delta != 0.) {
          sym r0=S.regvar("r0");
          sym lnX0=S.regvar("lnXh0");
          sym drdt = (r-r0)/delta;
          sym lnR0=S.regvar("log_R0");
          sym dlnRdt = (lnR-lnR0)/delta;
          sym dlnXdt = (lnX-lnX0)/delta - (drdt+r*dlnRdt)/S.rz*Dz(lnX);
          eq=dlnXdt-Ed/X*lap(X)+factor*eps/X+rhovr/rho*Dz(lnX); //news
                        } else {
          eq=-lap(X);//+factor*eps/X+rhovr/rho*Dz(lnX);
                               }
       sym_inited = true;
                       }
    S.set_value("lnXh",log(Xh));
    S.set_value("nuc.eps",nuc.eps);
    S.set_value("Wr",Wr); //new
    S.set_value("rho",rho); //new
    S.set_value("log_R",log(R)*ones(1,1)); //new
    if (delta != 0.) {
       S.set_value("lnXh0",log(Xh0));
       S.set_value("r0",r0); //new
       S.set_value("log_R0",log(R0)*ones(1,1)); //new
                     }
    S.set_map(map);
    eq.add(op,"lnXh","lnXh");
    eq.add(op,"lnXh","nuc.eps");
    eq.add(op,"lnXh","r");
    eq.add(op,"lnXh","Wr"); //new
    eq.add(op,"lnXh","log_R"); //new
    rhs=-eq.eval();

if (delta != 0.) { // unsteady case  ************************************
    matrix rhs_conv=-((log(Xh)-log(Xh0))/delta+factor*L_core/M_core/Xh);
    j0=0; 
    for(n=0;n<conv;n++) { // Take care of the CC domains
        ndom=map.gl.npts[n];
          op->reset(n,"lnXh");
          op->add_d(n,"lnXh","lnXh",ones(ndom,1)/delta);
          the_block=rhs_conv.block(j0,j0+ndom-1,0,0);
          rhs.setblock(j0,j0+ndom-1,0,0,the_block);
          j0+=ndom;
    } // end of loop on CC domains

//BC: continuity of lnXh at the core boundary
     op->bc_bot2_add_d(conv,"lnXh","lnXh",ones(1,1));
     op->bc_bot1_add_d(conv,"lnXh","lnXh",-ones(1,1));
     rhs(j0)=-log(Xh(j0))+log(Xh(j0-1));

//IC: continuity of lnXh and continuity of its derivative
 for(n=conv;n<ndomains-1;n++){
    ndom=map.gl.npts[n];
    j0+=ndom;
    op->bc_top1_add_d(n,"lnXh","lnXh",ones(1,1));
    op->bc_top2_add_d(n,"lnXh","lnXh",-ones(1,1));
    rhs(j0-1) = -log(Xh(j0-1))+log(Xh(j0));
    op->bc_bot1_add_l(n+1,"lnXh","lnXh",ones(1,1),map.D.block(n).row(-1));
    op->bc_bot2_add_l(n+1,"lnXh","lnXh",-ones(1,1),map.D.block(n+1).row(0));
    rhs(j0) = -(map.D,log(Xh))(j0-1)+(map.D,log(Xh))(j0);
	                     }
                 } else { // the steady case *******************************
     j0=0; 
     op->bc_bot2_add_l(0,"lnXh","lnXh",ones(1,1),map.D.block(0).row(0));
     rhs(0) = -(map.D,log(Xh))(0);
     for(n=0;n<ndomains-1;n++){
        ndom=map.gl.npts[n];
        j0+=ndom;
        op->bc_top1_add_d(n,"lnXh","lnXh",ones(1,1));
        op->bc_top2_add_d(n,"lnXh","lnXh",-ones(1,1));
        rhs(j0-1) = -log(Xh(j0-1))+log(Xh(j0));
        op->bc_bot1_add_l(n+1,"lnXh","lnXh",ones(1,1),map.D.block(n).row(-1));
        op->bc_bot2_add_l(n+1,"lnXh","lnXh",-ones(1,1),map.D.block(n+1).row(0));
        rhs(j0) = -(map.D,log(Xh))(j0-1)+(map.D,log(Xh))(j0);
                                 }
                        }
// Whatever the case, at the stellar surface Xh=X0:
     op->bc_top1_add_d(ndomains-1,"lnXh","lnXh",ones(1,1));
     rhs(-1)=-log(Xh(-1))+log(X0);

//fprintf(RHS," it = %d\n",glit);
//for (int k=0;k<nr;k++) fprintf(RHS,"RHS Xh %d, %e \n",k,rhs(k));
//fprintf(RHS,"RHS Xh END\n");

  op->set_rhs("lnXh",rhs);
//Evolution Xh end-----------------------------------
}

void star1d::solve_Wr(solver *op) {

// programmation en langage symbolique
    static symbolic S;
    static sym eq;
    static bool sym_inited = false;
    if (!sym_inited) { // Do it only the first time the function is called
    sym rho=S.regvar("rho");
    sym lnrhoc=S.regvar("log_rhoc");
// We use Wr=rho*Vr as the velocity variable. The use of Vr
// leads to huge Newton corrections and no convergence.
    sym Wr=S.regvar("Wr");
    sym lnR=S.regvar("log_R");
    sym r=S.r; // because "r" is created automatically with the symbolic object
    sym_vec rvec=grad(r);
    if (delta != 0.) {
       sym rho0=S.regvar("rho0");
       sym lnrhoc0=S.regvar("log_rhoc0");
       sym lnR0=S.regvar("log_R0");
       sym r0=S.regvar("r0");
       sym drdt = (r-r0)/delta;
       sym dlnRdt = (lnR-lnR0)/delta;
       sym drhodt = (rho-rho0)/delta - (drdt+r*dlnRdt)*Dz(rho)/S.rz;
       eq=drhodt+rho*(lnrhoc-lnrhoc0)/delta+div(Wr*rvec);
                     } else {
       eq=div(Wr*rvec);
                     }

    sym_inited=true;
    }
    S.set_value("Wr",Wr);
    S.set_value("rho",rho);
    S.set_value("log_R",log(R)*ones(1,1));
    S.set_value("log_rhoc",log(rhoc)*ones(1,1));
    S.set_map(map);
    if (delta != 0.) {
       S.set_value("log_R0",log(R0)*ones(1,1));
       S.set_value("log_rhoc0",log(rhoc0)*ones(1,1));
       S.set_value("rho0",rho0);
       S.set_value("r0",r0);
                     }
    eq.add(op,"Wr","Wr");
    eq.add(op,"Wr","rho");
    eq.add(op,"Wr","r");
    eq.add(op,"Wr","log_rhoc");
    eq.add(op,"Wr","log_R");
    matrix rhs=-eq.eval();


//BC
         op->bc_bot2_add_d(0,"Wr","Wr",ones(1,1));
         rhs(0)=-Wr(0);

//IC
	 int j0=0;
         for(int n=1;n<ndomains;n++){
           int ndom=map.gl.npts[n-1];
           j0+=ndom;
           op->bc_bot1_add_d(n,"Wr","Wr",ones(1,1));
           op->bc_bot2_add_d(n,"Wr","Wr",-ones(1,1));
           rhs(j0) = -Wr(j0-1)+Wr(j0);
         }

//fprintf(RHS," it = %d\n",glit);
//for (int k=0;k<nr;k++) fprintf(RHS,"RHS Wr %d, %e \n",k,rhs(k));
//fprintf(RHS,"RHS Wr END\n");
	op->set_rhs("Wr",rhs);
}


void star1d::new_solve_temp(solver *op) {
	//printf("start of new_solve temp \n");
	int n,j0,j1,ndom;
	char eqn[8];
	double fact;
	
	op->add_d("T","log_T",T);
	strcpy(eqn,"log_T");
	
	//Luminosity

	matrix rhs_lum,lum;
	
	lum=zeros(ndomains,1);
	j0=0;
        fact=4*PI*Lambda;
	for(n=0;n<ndomains;n++) {
	   ndom=map.gl.npts[n];
	   j1=j0+ndom-1;
	   if(n) lum(n)=lum(n-1);
	   lum(n)+=fact*(map.gl.I.block(0,0,j0,j1),(rho*nuc.eps*r*r).block(j0,j1,0,0))(0);
	   j0+=ndom;
	}

	rhs_lum=zeros(ndomains,1);
	j0=0;
	for(n=0;n<ndomains;n++) {
		ndom=map.gl.npts[n];
		j1=j0+ndom-1;
		op->bc_bot2_add_d(n,"lum","lum",ones(1,1));
		op->bc_bot2_add_li(n,"lum","rho",-fact*ones(1,1),map.gl.I.block(0,0,j0,j1),(r*r*nuc.eps).block(j0,j1,0,0));
		op->bc_bot2_add_li(n,"lum","nuc.eps",-fact*ones(1,1),map.gl.I.block(0,0,j0,j1),(r*r*rho).block(j0,j1,0,0));
		op->bc_bot2_add_d(n,"lum","Lambda",-4*PI*(map.gl.I.block(0,0,j0,j1),(rho*nuc.eps*r*r).block(j0,j1,0,0)));
		//r (rz)
		op->bc_bot2_add_li(n,"lum","r",-fact*ones(1,1),map.gl.I.block(0,0,j0,j1),(2*r*rho*nuc.eps).block(j0,j1,0,0));
		op->bc_bot2_add_li(n,"lum","rz",-fact*ones(1,1),map.gl.I.block(0,0,j0,j1),(r*r*rho*nuc.eps).block(j0,j1,0,0));
			
		if(n) op->bc_bot1_add_d(n,"lum","lum",-ones(1,1));
		j0+=ndom;
	}
	op->set_rhs("lum",rhs_lum);
	
	matrix Pe,xi_eff;
	
// We first compute the mask of Péclet numbers for convective/radiative domains
	Pe=zeros(nr,1);
	xi_eff=zeros(nr,1);
	j0=0;
	//domain_type[ndomains-1]=RADIATIVE; // last domain is imposed to be radiative
	for(n=0;n<ndomains;n++) {
		ndom=map.gl.npts[n];
		j1=j0+ndom-1;
		if (domain_type[n] == RADIATIVE) {
                   Pe.setblock(j0,j1,0,0,zeros(ndom,1));
    		} else if (domain_type[n] == CORE) {
              Pe.setblock(j0,j1,0,0,100.*Peclet*ones(ndom,1));
    		} else {
              Pe.setblock(j0,j1,0,0,Peclet*ones(ndom,1));
		}
		j0+=ndom;
	}
	xi_eff=(Pe+ones(nr,1))*opa.xi;

//Frad a completer
        matrix rhs_Frad,Frad,ss=entropy();

        Frad=-opa.xi*(D,T)-Pe*opa.xi*(D,entropy());
        rhs_Frad=zeros(ndomains*2-1,1);
        j0=0;
        for(n=0;n<ndomains;n++) {
                j1=j0+map.gl.npts[n]-1;

                if(n) op->bc_bot2_add_d(n,"Frad","Frad",ones(1,1));
                op->bc_top1_add_d(n,"Frad","Frad",ones(1,1));

                if(n) op->bc_bot2_add_l(n,"Frad","T",opa.xi.row(j0),D.block(n).row(0));
                op->bc_top1_add_l(n,"Frad","T",opa.xi.row(j1),D.block(n).row(-1));

                if(n) op->bc_bot2_add_l(n,"Frad","s",(Pe*opa.xi*T).row(j0),D.block(n).row(0));
                op->bc_top1_add_l(n,"Frad","s",(Pe*opa.xi*T).row(j1),D.block(n).row(-1));

                if(n) op->bc_bot2_add_d(n,"Frad","T",(Pe*opa.xi*(D,ss)).row(j0));
                op->bc_top1_add_d(n,"Frad","T",(Pe*opa.xi*(D,ss)).row(j1));

                if(n) op->bc_bot2_add_d(n,"Frad","opa.xi",((D,T)+Pe*T*(D,ss)).row(j0));
                op->bc_top1_add_d(n,"Frad","opa.xi",((D,T)+Pe*T*(D,ss)).row(j1));

                if(n) op->bc_bot2_add_d(n,"Frad","rz",Frad.row(j0));
                op->bc_top1_add_d(n,"Frad","rz",Frad.row(j1));

                j0=j1+1;
        }
        op->set_rhs("Frad",rhs_Frad);

	
	//Temperature
	
	matrix rhs_T,rhs_Lambda;

FILE *qfic=fopen("Pe.txt", "a");
fprintf(qfic," it = %d\n",glit);
for (int k=0;k<ndomains;k++) fprintf(qfic,"%d dom_type=%d\n",k,domain_type[k]);
for (int k=0;k<nr;k++) fprintf(qfic,"%d Pe=%e xi_eff=%e r=%e\n",k,Pe(k),xi_eff(k),map.r(k));
fclose(qfic);

// Now we write the temperature equation	
	rhs_T=zeros(nr,1);

	symbolic S;
	sym T_,xi_e,xi_t,lnP,del_ad;
	sym div_Flux;
	
	S.set_map(map);
	
	T_=S.regvar("T");
	lnP=S.regvar("log_p");
	xi_e=S.regvar("xi_eff");
	xi_t=S.regvar("xi_turb");
	del_ad=S.regvar("del_ad");
	S.set_value("T",T);
	S.set_value("xi_eff",xi_eff);
	S.set_value("xi_turb",Pe*opa.xi);
	S.set_value("log_p",log(p));
	S.set_value("del_ad",eos.del_ad);

	//div_Flux=-div(-xi_e*grad(T_))/xi_e-div(xi_t*del_ad*T_*grad(lnP))/xi_e;
	div_Flux=div(xi_e*grad(T_))/xi_e-div(xi_t*del_ad*T_*grad(lnP))/xi_e;
	
	div_Flux.add(op,eqn,"T",ones(nr,1));
	div_Flux.add(op,eqn,"log_p",ones(nr,1));
	div_Flux.add(op,eqn,"xi_eff",ones(nr,1));
	div_Flux.add(op,eqn,"xi_turb",ones(nr,1));
	div_Flux.add(op,eqn,"r",ones(nr,1));
	rhs_T-=div_Flux.eval();

	
	op->add_d(eqn,"nuc.eps",Lambda*rho/xi_eff);
	op->add_d(eqn,"rho",Lambda*nuc.eps/xi_eff);	
	op->add_d(eqn,"Lambda",rho*nuc.eps/xi_eff);
	op->add_d(eqn,"xi_eff",-Lambda*rho*nuc.eps/xi_eff/xi_eff);
	rhs_T+=-Lambda*rho*nuc.eps/xi_eff;
	
	
// Initialize the RHS of Lambda equation
	rhs_Lambda=zeros(ndomains,1);
	
// Scan now all the domains to impose the interface conditions according to the
// domain type.
	j0=0;
	for(n=0;n<ndomains;n++) {
		ndom=map.gl.npts[n];
		j1=j0+ndom-1;
		//printf("n = %d, domain_type=%d\n",n,domain_type[n]);
                if(n==0) { // care of the first and central domain
                        op->bc_bot2_add_d(n,eqn,"T",ones(1,1));
                        rhs_T(j0)=1.-T(j0);
			  
// MR: if there are chemical or turbulent discontinuities then impose the continuity of
//     the local flux rather than the temperature derivative
/*			  op->bc_top1_add_l(n,eqn,"T",xi_eff.row(j1),D.block(n).row(-1));
			  op->bc_top1_add_d(n,eqn,"xi_eff",(D,T).row(j1));
			  op->bc_top2_add_l(n,eqn,"T",-xi_eff.row(j1+1),D.block(n+1).row(0));
			  op->bc_top2_add_d(n,eqn,"xi_eff",-(D,T).row(j1+1));
			  op->bc_top1_add_d(n,eqn,"rz",-xi_eff(j1)*(D,T).row(j1));
			  op->bc_top2_add_d(n,eqn,"rz",xi_eff(j1+1)*(D,T).row(j1+1));
			  rhs_T(j1)=-xi_eff(j1)*(D,T)(j1)+xi_eff(j1+1)*(D,T)(j1+1);
*/
                        op->bc_top1_add_l(n,eqn,"T",ones(1,1),D.block(n).row(-1));
                        op->bc_top2_add_l(n,eqn,"T",-ones(1,1),D.block(n+1).row(0));
                        op->bc_top1_add_d(n,eqn,"rz",-(D,T).row(j1));
                        op->bc_top2_add_d(n,eqn,"rz",(D,T).row(j1+1));
                        rhs_T(j1)=-(D,T)(j1)+(D,T)(j1+1);


// MR: the variations of rz during the iterations are crucial to take intot account the 
//     changes of the mapping due to the distribution of domains that equalize the pressure
//     or temperature drop in a domain. In fine, when converged rz=1 in 1D, but rz should
//     be allowed to vary during iterations.

                           op->bc_bot2_add_l(n,"Lambda","T",ones(1,1),D.block(0).row(0));
                           rhs_Lambda(0)=-(D,T)(0);
	/*	
			op->bc_top1_add_d(n,"Lambda","Frad",4*PI*(r*r).row(j0));
                        op->bc_top1_add_d(n,"Lambda","r",4*PI*(Frad*2*r).row(j0));
                        op->bc_top2_add_d(n,"Lambda","lum",-ones(1,1));
                        rhs_Lambda(1)=-4*PI*Frad(j1)*(r*r)(j0)+lum(n+1);
 	*/

                } else if (n==ndomains-1) { // care of the last domain
                        op->bc_bot2_add_d(n,eqn,"T",ones(1,1));
                        op->bc_bot1_add_d(n,eqn,"T",-ones(1,1));
                        rhs_T(j0)=-T(j0)+T(j0-1);
			op->bc_top1_add_d(n,eqn,"T",ones(1,1));
			op->bc_top1_add_d(n,eqn,"Ts",-ones(1,1));
			rhs_T(-1)=Ts(0)-T(-1);
                        op->bc_bot2_add_d(n,"Lambda","Lambda",ones(1,1));
                        op->bc_bot1_add_d(n,"Lambda","Lambda",-ones(1,1));
                        //if (details) printf("LAST DOM n=%d T bot T top \n",n);
                } else { // Now domains are not first and not last!
	               // We first care about radiative domains
			// Case of Lambda equation
                           op->bc_bot2_add_d(n,"Lambda","Lambda",ones(1,1));
                           op->bc_bot1_add_d(n,"Lambda","Lambda",-ones(1,1));
			// Now the temperature equation
			// we apply continuity of T at bottom and of DT at top
                           op->bc_bot2_add_d(n,eqn,"T",ones(1,1));
                           op->bc_bot1_add_d(n,eqn,"T",-ones(1,1));
                           rhs_T(j0)=-T(j0)+T(j0-1);
/*			   op->bc_top1_add_l(n,eqn,"T",xi_eff.row(j1),D.block(n).row(-1));
			   op->bc_top1_add_d(n,eqn,"xi_eff",(D,T).row(j1));
			   op->bc_top2_add_l(n,eqn,"T",-xi_eff.row(j1+1),D.block(n+1).row(0));
			   op->bc_top2_add_d(n,eqn,"xi_eff",-(D,T).row(j1+1));
			   op->bc_top1_add_d(n,eqn,"rz",-xi_eff(j1)*(D,T).row(j1));
			   op->bc_top2_add_d(n,eqn,"rz",xi_eff(j1+1)*(D,T).row(j1+1));
			   rhs_T(j1)=-xi_eff(j1)*(D,T)(j1)+xi_eff(j1+1)*(D,T)(j1+1);
*/
                        op->bc_top1_add_l(n,eqn,"T",ones(1,1),D.block(n).row(-1));
                        op->bc_top2_add_l(n,eqn,"T",-ones(1,1),D.block(n+1).row(0));
                        op->bc_top1_add_d(n,eqn,"rz",-(D,T).row(j1));
                        op->bc_top2_add_d(n,eqn,"rz",(D,T).row(j1+1));
                        rhs_T(j1)=-(D,T)(j1)+(D,T)(j1+1);
		} // End of options on n==0, n==ndomains-1, else
		j0+=ndom;
	}  // End of loop on domains rank

	op->set_rhs(eqn,rhs_T);
	op->set_rhs("Lambda",rhs_Lambda);

}
//--------------------------END of NEW_solve_temp-------------------------------------------


void star1d::solve_dim(solver *op) {
    int n,j0;
    matrix q,rhs;

    rhs=zeros(ndomains,1);
    j0=0;
    for(n=0;n<ndomains;n++) {
        op->bc_bot2_add_d(n,"m","m",ones(1,1));
        //rho
        op->bc_bot2_add_li(n,"m","rho",-4*PI*ones(1,1),map.gl.I.block(0,0,j0,j0+map.gl.npts[n]-1),(r*r).block(j0,j0+map.gl.npts[n]-1,0,0));
        //r (rz)
        op->bc_bot2_add_li(n,"m","r",-4*PI*ones(1,1),map.gl.I.block(0,0,j0,j0+map.gl.npts[n]-1),(2*r*rho).block(j0,j0+map.gl.npts[n]-1,0,0));
        op->bc_bot2_add_li(n,"m","rz",-4*PI*ones(1,1),map.gl.I.block(0,0,j0,j0+map.gl.npts[n]-1),(r*r*rho).block(j0,j0+map.gl.npts[n]-1,0,0));

        if(n) op->bc_bot1_add_d(n,"m","m",-ones(1,1));
        j0+=map.gl.npts[n];
    }
    op->set_rhs("m",rhs);

    for(n=0;n<ndomains;n++) {
        op->add_d(n,"log_rhoc","log_pc",1./eos.chi_rho(0)*ones(1,1));
        op->add_d(n,"log_rhoc","log_Tc",-eos.d(0)*ones(1,1));
    }


    rhs=zeros(ndomains,1);
    for(n=0;n<ndomains;n++) {
        if(n==ndomains-1) {
            op->add_d(n,"log_pc","log_pc",ones(1,1));
            op->add_d(n,"log_pc","pi_c",ones(1,1)/pi_c);
            op->add_d(n,"log_pc","log_rhoc",-2*ones(1,1));
            op->add_d(n,"log_pc","log_R",-2*ones(1,1));
        } else {
            op->bc_top1_add_d(n,"log_pc","log_pc",ones(1,1));
            op->bc_top2_add_d(n,"log_pc","log_pc",-ones(1,1));
        }
    }
    op->set_rhs("log_pc",rhs);

    rhs=zeros(ndomains,1);
    for(n=0;n<ndomains;n++) {
        if(n==ndomains-1) {
            op->add_d(n,"log_Tc","log_Tc",ones(1,1));
            op->add_d(n,"log_Tc","log_rhoc",-ones(1,1));
            op->add_d(n,"log_Tc","Lambda",ones(1,1)/Lambda);
            op->add_d(n,"log_Tc","log_R",-2*ones(1,1));
        } else {
            op->bc_top1_add_d(n,"log_Tc","log_Tc",ones(1,1));
            op->bc_top2_add_d(n,"log_Tc","log_Tc",-ones(1,1));
        }
    }
    op->set_rhs("log_Tc",rhs);

    rhs=zeros(ndomains,1);
    for(n=0;n<ndomains;n++) {
        if(n==ndomains-1) {
            op->add_d(n,"log_R","log_R",3*ones(1,1));
            op->add_d(n,"log_R","m",1/m*ones(1,1));
            op->add_d(n,"log_R","log_rhoc",ones(1,1));
        } else {
            op->bc_top1_add_d(n,"log_R","log_R",ones(1,1));
            op->bc_top2_add_d(n,"log_R","log_R",-ones(1,1));
        }
    }
    op->set_rhs("log_R",rhs);

//fprintf(RHS," it = %d\n",glit);
//for (int k=0;k<ndomains;k++) fprintf(RHS,"RHS log_R %d, %e \n",k,rhs(k));
//fprintf(RHS,"RHS R END\n");
}
//------------------------------------------------------------------------
void star1d::solve_map(solver *op) {
	int n,j0,j1,ndom;
	matrix rhs;
	
	rhs=zeros(ndomains,1);
	for(n=0;n<ndomains;n++) {
		op->bc_top1_add_d(n,"dRi","dRi",ones(1,1));
		op->bc_top1_add_d(n,"dRi","Ri",ones(1,1));
		if(n<ndomains-1) op->bc_top2_add_d(n,"dRi","Ri",-ones(1,1));
	}	
	op->set_rhs("dRi",rhs);
	
	
	rhs=zeros(ndomains,1);
	int nzones=zone_type.size();

	j0=0;
	for(n=0;n<ndomains;n++) {
// place the domains radii "Ri" so as to equalize the PRES drop,
// between 2 adjacent domains
		ndom=map.gl.npts[n];
		if(n==0) op->add_d(n,"Ri","Ri",ones(1,1));
		else {
			matrix delta;
			j1=j0+ndom-1;
			delta=zeros(1,map.gl.npts[n]); delta(0)=1;delta(-1)=-1;
			op->bc_bot2_add_l(n,"Ri",LOG_PRES,ones(1,1),delta);
			delta=zeros(1,map.gl.npts[n-1]); delta(0)=1;delta(-1)=-1;
			op->bc_bot1_add_l(n,"Ri",LOG_PRES,-ones(1,1),delta);
			rhs(n)=log(PRES(j1))-log(PRES(j0))-log(PRES(j0-1))+log(PRES(j0-map.gl.npts[n-1]));
		}
		j0+=ndom;
	}
	
fprintf(RHS," it = %d\n",glit);
for (int k=0;k<ndomains;k++) fprintf(RHS,"RHS Ri %d, %e \n",k,rhs(k));
fprintf(RHS,"RHS Ri END avec nzones=1\n");
	
	if(nzones>1) {
		
		symbolic S;
		sym p_,s_,eq;
		p_=S.regvar("p"); s_=S.regvar("s");
		S.set_map(map); S.set_value("p",p); S.set_value("s",entropy());

matrix ss=entropy();
fprintf(RHS," it = %d\n",glit);
for (int k=0;k<nr;k++) fprintf(RHS,"entropy %d, %e \n",k,ss(k));
fprintf(RHS,"nzones = %d\n",nzones);
for (int k=0;k<nzones;k++) fprintf(RHS,"izif %d, %d \n",k,izif[k]);
for (int k=0;k<ndomains;k++) fprintf(RHS,"Ri %d, %e \n",k,map.R(k));
	
		//eq=(grad(p_),grad(s_))/S.r/S.r;
		eq=(grad(p_),grad(s_));
	
// Take care of convective layers
// number of zones=number of interface+1 (the surface)

		for (int iz=0;iz<nzones-1;iz++) { // we scan the zone interfaces
		        for(n=0,j0=0;n<izif[iz]+1;n++) j0+=map.gl.npts[n];
			// j0 is the radial index of the interface
			n=izif[iz]+1; // n is index of the domain just above the interface
		        op->reset(n,"Ri");
			eq.bc_bot2_add(op,n,"Ri","p",ones(1,nth));
			eq.bc_bot2_add(op,n,"Ri","s",ones(1,nth));
			eq.bc_bot2_add(op,n,"Ri","r",ones(1,nth));
		        rhs(n)=-eq.eval()(j0);	
			fprintf(RHS,"iz= %d, n=%d, j0= %d\n",iz,n,j0);
		}
fprintf(RHS,"Entropy END\n");
	}
	
fprintf(RHS," it = %d\n",glit);
for (int k=0;k<ndomains;k++) fprintf(RHS,"RHS Ri %d, %e \n",k,rhs(k));
fprintf(RHS,"RHS Ri END\n");
	op->set_rhs("Ri",rhs);
}

void star1d::solve_gsup(solver *op) {

// Insert the variational form of the equation g=GM/R^2=G*rho_c*R*m

    matrix g;
    int n=ndomains-1;

    g=gsup()*ones(1,1);
    op->bc_top1_add_d(n,"gsup","gsup",1./g);
    op->bc_top1_add_d(n,"gsup","log_rhoc",-ones(1,1));
    op->bc_top1_add_d(n,"gsup","log_R",-ones(1,1));
    op->bc_top1_add_d(n,"gsup","m",-ones(1,1)/m);

    op->set_rhs("gsup",zeros(1,1));

}

void star1d::solve_Teff(solver *op) {
    matrix q,Te,F;
    int n=ndomains-1;

    Te=Teff()*ones(1,1);
    F=SIG_SB*pow(Te,4);

    op->bc_top1_add_d(n,"Teff","Teff",4*SIG_SB*pow(Te,3));
    op->bc_top1_add_d(n,"Teff","log_Tc",-F);
    op->bc_top1_add_d(n,"Teff","log_R",F);
    op->bc_top1_add_d(n,"Teff","opa.xi",-F/opa.xi.row(-1));

    q=opa.xi*Tc/R;
    op->bc_top1_add_l(n,"Teff","T",q.row(-1),D.block(n).row(-1));

    //q=-(D,T)*opa.xi;
    //op->bc_top1_add_d(n,"Teff","rz",Tc/R*q.row(-1));
    op->bc_top1_add_d(n,"Teff","rz",F);

/*    op->bc_top1_add_d(n,"Teff","Teff",4./Te);
    op->bc_top1_add_d(n,"Teff","log_Tc",-ones(1,1));
    op->bc_top1_add_d(n,"Teff","log_R",ones(1,1));
    op->bc_top1_add_d(n,"Teff","lum",-ones(1,1)/luminosity()*R*Tc);*/


    op->set_rhs("Teff",zeros(1,1));

}


void star1d::check_jacobian(solver *op,const char *eqn) {
    star1d B;
    matrix rhs,drhs,drhs2,qq;
    matrix *y;
    double q;
    int i,j,j0;

    y=new matrix[op->get_nvar()];
    B=*this;
    // Perturbar el modelo
    {
        double a,ar,asc;

        a=1e-8;ar=1e-8;
        asc=a>ar?a:ar;
        B.phi=B.phi+a*B.phi+ar*B.phi*random_matrix(nr,1);
        B.p=B.p+a*B.p+ar*B.p*random_matrix(nr,1);
        B.pc=B.pc+asc*B.pc;
        B.T=B.T+a*B.T+ar*B.T*random_matrix(nr,1);
        B.Tc=B.Tc+asc*B.Tc;
        B.map.R=B.map.R+a*B.map.R+ar*B.map.R*random_matrix(ndomains,1);
        B.map.remap();
    }

    B.fill();

    i=op->get_id("rho");
    y[i]=zeros(nr,1);
    i=op->get_id("opa.xi");
    y[i]=zeros(nr,1);
    i=op->get_id("nuc.eps");
    y[i]=zeros(nr,1);
    i=op->get_id("r");
    y[i]=zeros(nr,1);
    i=op->get_id("rz");
    y[i]=zeros(nr,1);
    i=op->get_id("s");
    y[i]=zeros(nr,1);
    i=op->get_id("opa.k");
    y[i]=zeros(nr,1);


    i=op->get_id("Phi");
    y[i]=zeros(nr,1);
    y[i]=B.phi-phi;
    i=op->get_id("p");
    y[i]=B.p-p;
    i=op->get_id("log_p");
    y[i]=log(B.p)-log(p);
    i=op->get_id("pi_c");
    y[i]=(B.pi_c-pi_c)*ones(ndomains,1);
    i=op->get_id("T");
    y[i]=B.T-T;
    i=op->get_id("log_T");
    y[i]=log(B.T)-log(T);
    i=op->get_id("Lambda");
    y[i]=(B.Lambda-Lambda)*ones(ndomains,1);
    i=op->get_id("Ri");
    y[i]=zeros(ndomains,1);
    y[i].setblock(1,ndomains-1,0,0,(B.map.R-map.R).block(0,ndomains-2,0,0));
    j=i;
    i=op->get_id("dRi");
    y[i]=zeros(ndomains,1);
    y[i].setblock(0,ndomains-2,0,0,y[j].block(1,ndomains-1,0,0)-y[j].block(0,ndomains-2,0,0));
    y[i](ndomains-1)=-y[j](ndomains-1);
    i=op->get_id("log_rhoc");
    y[i]=(log(B.rhoc)-log(rhoc))*ones(ndomains,1);
    i=op->get_id("log_pc");
    y[i]=(log(B.pc)-log(pc))*ones(ndomains,1);
    i=op->get_id("log_Tc");
    y[i]=(log(B.Tc)-log(Tc))*ones(ndomains,1);
    i=op->get_id("log_R");
    y[i]=(log(B.R)-log(R))*ones(ndomains,1);
    i=op->get_id("m");
    q=0;
    j0=0;
    y[i]=zeros(ndomains,1);
    for(j=0;j<ndomains;j++) {
        q+=4*PI*(B.map.gl.I.block(0,0,j0,j0+map.gl.npts[j]-1),
            (B.rho*B.r*B.r).block(j0,j0+map.gl.npts[j]-1,0,0))(0)-
            4*PI*(map.gl.I.block(0,0,j0,j0+map.gl.npts[j]-1),
            (rho*r*r).block(j0,j0+map.gl.npts[j]-1,0,0))(0);
        y[i](j)=q;
        j0+=map.gl.npts[j];
    }
    i=op->get_id("ps");
    y[i]=B.ps-ps;
    i=op->get_id("Ts");
    y[i]=B.Ts-Ts;
    i=op->get_id("lum");
    y[i]=zeros(ndomains,1);
    j0=0;
    q=0;
    for(j=0;j<ndomains;j++) {
        q+=4*PI*B.Lambda*(B.map.gl.I.block(0,0,j0,j0+map.gl.npts[j]-1),
            (B.rho*B.nuc.eps*B.r*B.r).block(j0,j0+map.gl.npts[j]-1,0,0))(0)-
            4*PI*Lambda*(map.gl.I.block(0,0,j0,j0+map.gl.npts[j]-1),
            (rho*nuc.eps*r*r).block(j0,j0+map.gl.npts[j]-1,0,0))(0);
        y[i](j)=q;
        j0+=map.gl.npts[j];
    }
    i=op->get_id("Frad");
    y[i]=zeros(ndomains*2-1,1);
    j0=0;
    matrix Frad,BFrad;
    Frad=-opa.xi*(D,T);
    BFrad=-B.opa.xi*(B.D,B.T);
    for(j=0;j<ndomains;j++) {
        if(j) y[i](2*j-1)=BFrad(j0)-Frad(j0);
        y[i](2*j)=BFrad(j0+map.gl.npts[j]-1)-Frad(j0+map.gl.npts[j]-1);
        j0+=map.gl.npts[j];
    }
    i=op->get_id("gsup");
    y[i]=(B.gsup()-gsup())*ones(1,1);
    i=op->get_id("Teff");
    y[i]=(B.Teff()-Teff())*ones(1,1);

    B.solve(op);
    rhs=op->get_rhs(eqn);
    B=*this;
    B.solve(op);

    op->mult(y);
    drhs=rhs-op->get_rhs(eqn);
    drhs2=y[op->get_id(eqn)]+drhs;

    // static figure fig("/XSERVE");

    drhs.write();
    drhs2.write();

    // fig.axis(0-nn/20.,nn*(1+1./20),-15,0);
    // fig.plot(log10(abs(drhs)+1e-20),"b");
    // fig.hold(1);
    // fig.plot(log10(abs(drhs2)+1e-20),"r");
    // fig.hold(0);

    delete [] y;
}

matrix calcMass(star1d &a) {

        solver massSolver;
        massSolver.init(a.map.ndomains, 1, "full");
        massSolver.regvar("mass");
        massSolver.set_nr(a.map.npts);

        massSolver.add_l("mass", "mass", ones(a.nr, 1), a.D);
        matrix rhs = 4 * PI * a.r * a.r * a.rho;

        massSolver.bc_bot2_add_d(0, "mass", "mass", ones(1, 1));
        rhs(0) = 0;
        int jfirst = 0;
        for(int i = 1; i < a.ndomains; i++) {
                jfirst += a.map.gl.npts[i-1];
                massSolver.bc_bot2_add_d(i, "mass", "mass", ones(1, 1));
                massSolver.bc_bot1_add_d(i, "mass", "mass", -ones(1,
1));
                rhs(jfirst) = 0;
        }
        massSolver.set_rhs("mass", rhs);
        massSolver.solve();

        matrix mass = massSolver.get_var("mass");

        return mass;

}


