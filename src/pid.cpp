#include "pid.hpp"
#include "position.hpp"
#include "encoder.hpp"
#include "config.hpp"
#include <queue>
#include <math.h>

// Durée de la rampe en seconde
#define RAMPE_LENGHT 2.0f

/* --- PID général --- */

PID::PID(){} //kewa?

PID::PID(float _p, float _i, float _d, float _erreurSeuil, float _accumulateurSeuil, Position* position){

    p = _p;
    i = _i;
    d = _d;

    accumulateur = 0;
    actionFinished = 0;

    pos = position;

    accumulateurSeuil = _accumulateurSeuil;
    erreurSeuil = _erreurSeuil;
    time.start();
}

float PID::calculerConsigne(){
    // TODO : Ajouter la dérivée
    return (p * erreur + i * accumulateur) * getRampe();
}

void PID::AccumulerErreur(float erreur){
    if (accumulateur < accumulateurSeuil) accumulateur += erreur;
}

float PID::getConsigne(){
    erreur = this->calculerErreur();
    if (erreur < erreurSeuil) {
        actionFinished = 1;
        printf("TRIGGER");
    }
    AccumulerErreur(erreur);
    return calculerConsigne();
}

float PID::getRampe(){
    if (time.read() <  RAMPE_LENGHT) return time.read() / RAMPE_LENGHT;
    return 1;
}

void PID::reset(){
    accumulateur = 0;
    time.reset();
}

/* --- PID en DISTANCE --- */

PIDDistance::PIDDistance(){}

PIDDistance::PIDDistance(float _p, float _i, float _d, float _erreurSeuil, float _accumulateurSeuil, Position* position) :
    PID(_p, _i, _d, _erreurSeuil, _accumulateurSeuil, position) {}

// FIXME : Fonction mal placée
float carre(float a){
    return a * a;
}

float PIDDistance::calculerErreur(){
   float x = pos->get_x();
   float y = pos->get_y();

   float err = sqrt(carre(x - commande_x) + carre(y - commande_y));
   fifo.push(err);
   fifo.pop();
   return err;
}

void PIDDistance::setCommande(float x, float y){
    commande_x = x;
    commande_y = y;
    PID::reset();
    this->time.reset();
}

/* --- PID en ANGLE --- */

PIDAngle::PIDAngle(){}

PIDAngle::PIDAngle(float _p, float _i, float _d, float _erreurSeuil, float _accumulateurSeuil, Position* position) :
    PID(_p, _i, _d, _erreurSeuil, _accumulateurSeuil, position) {}

float PIDAngle::calculerErreur(){
   float theta = pos->get_theta();
   float err = commande_theta - theta; // TODO vérifier le sens !
   fifo.push(err);
   fifo.pop();
   // printf("Erreur Angle : %f, comm = %f, theta = %f\r\n", err, commande_theta, theta);
   return err;
}

void PIDAngle::setCommande(float theta){
   commande_theta = theta;
   PID::reset();
}

float calculerAngle(float x1, float y1, float x2, float y2)
{
    if ((y2-y1) < 0.001f) return 0;

    return asin((x2-x1)/(y2-y1));
}
