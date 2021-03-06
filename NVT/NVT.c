/****************************************************************************
BASIC NVT MONTE CARLO
*****************************************************************************  
HOW TO USE THIS PROGRAM:
************************
You need a .txt file with initial coordinates for your atoms. If you got this
from me, you can use startgenerator to do this.
startgenerator only works well with particle numbers with integer cube roots (or
2 particles,) so this program has that constraint as well. 

-Luciano Laratelli
luciano.e.laratelli@outlook.com
***************************************************************************/

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct particle
{
    double x[3];
};

struct move_values
{
    int p;
    double delta, gamma, zeta;
};

/***************
IF COMPILING IN LINUX, THESE CONST INTS MUST BE CHANGED TO USE #define INSTEAD, i.e
#define N 1000

If you know why this is the case, please let me know!
****************/

//this is the NVT part
const int N = 1000; //number of particles
const double T = 77; //kelvin; 
const int L = 200; //length of one side of the cube, L = sigma
const double k = 1.0;//boltzmann factor


/*
#define N 1000
#define T 10
#define L 20
*/

//we declare our structs so we can use them to do our bidding
struct particle particles[N];
struct move_values move;

/*******************
starting_positions takes a .txt file with coordinates 
for the starting positions of the particles in this simulation.
each line of the file is an (x,y,z) with format x y z\n
*******************/
void starting_positions()
{
    int p;
    FILE * startingpositions; 
    startingpositions = fopen("startingpositions.txt", "r");
    for(p=0;p<N;p++)
    {
        fscanf(startingpositions,"%lf %lf %lf\n", &particles[p].x[0],&particles[p].x[1],&particles[p].x[2]);
    }
    fclose(startingpositions);
    return;
}

/************************
distfinder finds the distance between any two distinct particles of
number "id_a" and "id_b" respectively. this distance is used to
calculate the potential energy in PEfinder 
************************/
double distfinder(int id_a, int id_b)
{
    int i;
    double dist, delta2,delta;
    double cutoff = 0.5 * L;
    delta2 = 0; //clearing junk from memory just in case
    for(i=0;i<3;i++)
    {
        delta = fabs(particles[id_a].x[i] - particles[id_b].x[i]);
        if(delta > cutoff)
        {
            delta -= cutoff;
        }
        delta2 += delta * delta;
        //finds distance coordinate by coordinate,
        //keeping the sum of the squares in memory
    }
    dist = sqrt(delta2); 
    //distance formula says dist = sqrt of the sum of the squares
    return dist;
}

/***m*******************
PEfinder finds the total potential energy between every particle
by using the lennard-jones potential
lennard-jones is one person, if you were wondering
**********************/
double PEfinder()
{
    double s2,s6,s12;//exponents of sigma
    double rinv,r2,r6,r12;//exponents of r
    double sor6,sor12;//sigma over r => s/r
    double epsilon = 128.326802,
           sigma = 3.371914,
           pe = 0,
           r; 
    s2 = sigma * sigma;
    s6 = s2*s2*s2;
    s12 = s6*s6;
    double cutoff = L * 0.5;
    for(int b=0;b<N;b++)
    {
        for(int c=b+1;c<N;c++)
        {
            r = distfinder(b,c);
            if(r<cutoff)
            {
                continue;
            }
            rinv = 1/r;
            //formula has sigma/r; this is easier to work with
            r2 = rinv * rinv;
            r6 = r2*r2*r2;
            r12 = r6* r6;
            sor6 = r6 * s6;
            sor12 = r12 * s12;
            //makes it harder to mess up the formula, h/t adam for the tip
            pe += (4 * epsilon * (sor12-sor6));
        }
    }
    return pe;
}

/**********************
Positionchecker is called by rand_p_mover and rand_p_unmover
it looks at every particle p in the system; if rand_p moves p out of the box,
positionchecker makes it so that the particle "pops in" from the other side
of the box. think pacman or snake.

Why does position checker have allowed values of [0,20) ? The distinction is arbitrary,
but one of the two "edges" must be excluded from the allowed values.
**********************/

void positionchecker(int id_a)
{
    int i;
    for(i=0;i<3;i++)
    {
        if(particles[id_a].x[i] >= L)
        {
            particles[id_a].x[i] -= L;
        }
        if(particles[id_a].x[i] < 0)
        {
            particles[id_a].x[i] += L;
        }
    }
    return;
}


/**************************
 uniformrand returns a random number between 1 and 0; its output can be modified to fit
 any range, as seen in rand_p_mover just below it.
***************************/
double uniformrand()
{
    static int first_time_through = 1;
    if(first_time_through)
    {
        srandom(time(NULL));
        first_time_through = 0;
    }
    double r = random();
    return r / ((double)RAND_MAX + 1);
}


/*********************
this next function is the first step of the monte carlo method; 
it moves a random particle a random distance away from its current position
and then calls positionchecker to enforce boundary conditions
*********************/
void rand_p_mover()
{
    int p = random() / (RAND_MAX / N + 1); //p is a random int between 0 and N
    double delta = (uniformrand() - 0.5)*L, //a random float between -L/2 and L/2 
           gamma = (uniformrand() - 0.5)*L,   //same as above
           zeta  = (uniformrand() - 0.5)*L;  //same as above
    move.p = p; //storing values of p, delta, gamma, and zeta in the struct 
    move.delta = delta; //in case the move is rejected
    move.gamma = gamma;
    move.zeta = zeta;
    particles[p].x[0] += delta; //making the move
    particles[p].x[1] += gamma; //same
    particles[p].x[2] += zeta; //same
    //we can't leave the box (nor do we want to be at the surface;)
    //if p manages to escape the box, positionchecker bosses it around 
    positionchecker(p);
    return;
}


/********************
this is the second part of the monte carlo method; it takes the PE from PE finder
for the current configuration and subtracts it from the previous configuration. 
it substitutes delta E into the e^-beta equation and compares the value of
THAT exponential to a random number between 0 and 1
if the result of the exponential is greater than the random number, the change
is accepted
********************/
bool E_checker(double cpe, double npe,int c)
{
    FILE * energies;
    energies = fopen("energies.dat","a");
    double deltaE = npe - cpe,
           guess,
           beta = 1 / (k * T),
           prob; //no ints in this function, no sir!
    //DELTA between two things is FINAL MINUS INITIAL
    //NOT THE OTHER WAY AROUND
    if(deltaE < 0)//if the new energy is lower than the old energy, we always accept it
    {
        fprintf(energies,"%d %f\n",c, npe); 
        fclose(energies);
        return true;
    }
    guess=((double)random()/(double)RAND_MAX);
    prob = exp(-1*beta*deltaE);
    if(prob > guess)
    {
        fprintf(energies,"%d %f\n",c, npe); 
        fclose(energies);
        return true;
    }
    else
    {
        fprintf(energies,"%d %f\n",c, cpe); 
        fclose(energies);
        return false;
    }
}


/****************
rand_p_unmover does something very important: if a move is rejected by E_checker, 
rand_p_unmover reverts the system to its previous state, using the struct move_values
****************/
void rand_p_unmover()
{
    int p = move.p;//remembering the particle we want to unmove
    particles[p].x[0] -= move.delta;
    particles[p].x[1] -= move.gamma;
    particles[p].x[2] -= move.zeta;
    positionchecker(p);
    return;
}


/******************
output_to_file lives up to its name;
It ouputs the coordinates of the particles at every frame into an .xyz file, so we 
can visualize the system using software like VMD
*******************/
void output_to_file()
{
    FILE * positions;
    positions = fopen("positions.xyz","a");
    fprintf(positions, "%d \n\n",N);
    for(int p=0;p<N;p++)
    {
        fprintf(positions,"Ar %lf %lf %lf\n", particles[p].x[0], particles[p].x[1], particles[p].x[2]);
        //hoping this works lmao
        //update: it works!
    }
    fclose(positions);
    return;
}


double helmholtz(double new_energy, double past_energy, int c)
{
    FILE * free_energies = fopen("free_energies.dat","a");
    double delta = new_energy - past_energy,
           beta = 1 / ( k * T),
           expo = exp(-1 * delta * beta),
           helmholtz = k*T*log(expo/c);     
    fprintf(free_energies,"%d %lf\n",c,helmholtz);
    fclose(free_energies);
    return expo;
}


int main()
{
    clock_t begin = clock(); //so we know how long our program takes
    FILE * positions = fopen("positions.xyz","w");
    FILE * energies = fopen("energies.dat","w");
    FILE * free_energies = fopen("free_energies.dat","w");
    fclose(positions);//don't need this for now so we close it
    fclose(free_energies);
    bool guess; 
    int c; //count
    int m; //maximum number of tries
    printf("How many tries do you want to do?\n"); //user-directed!
    scanf("%d", &m);
    starting_positions(); //read in starting positions
    double cpe = PEfinder(),//current potential energy
           npe;
    double sum = cpe,
           sum_past_energy = 0.0,
           average; //the sum lets us find the average
    output_to_file(); //writes positions to a file
    fprintf(energies,"0 %f\n", cpe); 
    fclose(energies);//won't need this until later so we close it
    for(c=1;c<m;c++)
    {
        rand_p_mover(); //monte carlo step one
        npe = PEfinder(); //"new potential energy"
        guess = E_checker(cpe,npe,c); 
        if (guess == true) //if new energy is less than the random number, accept
        {
            output_to_file(); //for just the positions
            double past_energy = cpe;
            cpe = npe; //updates energy
            sum_past_energy += past_energy;
            sum += cpe;//accounting
            helmholtz(sum, sum_past_energy,c); 
        }
        else //hopefully self-explanatory
        {
            //have to actually reject the move!
            rand_p_unmover(); //we reject the move by undoing it
            output_to_file(); //for just the positions (from before the move, i.e.
                              //system stays the same for another frame)
            sum += cpe;
            helmholtz(sum,sum,c);
            continue;
        }
    }
    average = sum / (m); //again, hopefully self explanatory
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Done! Hope it worked out. \nThis run took %f seconds.\nThe average energy was %f.\nHave a nice day!\n",time_spent, average); //it never does
    return 0;
}
