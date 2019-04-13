#include "navigateur.hpp"
#include <stdio.h>
#include <math.h>
#include "../base.hpp"
#include "../config.hpp"

extern int debug_monitor;
extern int move;

Navigateur::Navigateur(Position *_position, PwmOut *_m_l, PwmOut *_m_r, DigitalOut *_d_l, DigitalOut *_d_r, Encoder * encod_l, Encoder * encod_r)
{
    position = _position;
    cible = NULL;

    // FIXME : Trouver bonnes valeurs de pid.
    PIDDistance _pid_d(0.001, 0.01, 0.1, 0.01, 1, position);
    PIDAngle _pid_a(0.03, 0.02, 0.1, 0.02, 0, position);
    PIDVitesse _pid_v_l(0.03, 0.02, 0.1, 0.02, 0, encod_l, 1);
    PIDVitesse _pid_v_r(0.03, 0.02, 0.1, 0.02, 0, encod_r, 1);

    pid_d = _pid_d;
    pid_a = _pid_a;
    pid_v_l = _pid_v_l;
    pid_v_r = _pid_v_r;

    m_l = _m_l;
    m_r = _m_r;
    m_l->period(PWM_PERIOD);
    m_r->period(PWM_PERIOD);
    d_l = _d_l;
    d_r = _d_r;
}

void Navigateur::set_destination(Vecteur2D *c)
{
    cible = c;
    // Cible sert de stokage de la consigne
    pid_d.setCommande(cible->x(), cible->y());
}

void Navigateur::reset() {
    position->reset();
    pid_a.reset();
    pid_d.reset();
    Vecteur2D destination(0, 0);
    set_destination(&destination);
}

void limiter_consigne(float* consigne, int *direction)
{
    if (*consigne < 0) {
        *direction == 1 ? *direction = 0 : *direction = 1;
        *consigne = abs(*consigne);
    }
    // Commenté car géré directement dans la fonction update
    // consigne = min(*consigne, CONSIGNE_MAX);
    *consigne = min(*consigne, 0.4f);
    *consigne = max(*consigne, -0.4f);
}

float min (float a, float b)
{
    return (a > b) ? b : a;
}

float max (float a, float b)
{
    return (a < b) ? b : a;
}

void Navigateur::update()
{

    float x = position->get_x();
    float y = position->get_y();
    float theta = position->get_theta();
    
    // Initialisation des consignes
    float angle_cons = 0;
    float dist_cons = 0;
    float angle_relatif = 0;
    float angle_absolu_destination = theta;
    int i = 1;
    int dir_l;
    int dir_r;
    float cmr;
    float cml;


    int triggered = 0; // False

    // printf("x = %f, y = %f, theta=%f\n", x, y, theta);
    // printf("%f, %f, %f\r\n", x, y, theta);
    // printf("Accumulateurs : (Dist : %f) (Angle : %f)\r\n", pid_d.accumulateur, pid_a.accumulateur);

    float distance_cible = sqrt(carre(x - cible->x())+ carre(y - cible->y()));

    if (distance_cible > 0.1) {
        angle_absolu_destination = modulo_angle_absolu(calculerAngle(x, y, cible->x(), cible->y()));
    } else {
        angle_absolu_destination = theta;
    }

    angle_relatif = modulo_angle_relatif(angle_absolu_destination - theta);

    if ((abs(angle_relatif) < 0.1) || (abs(abs(angle_relatif) - PI) < 0.1)) {
        triggered = 1;
        dist_cons = pid_d.getConsigne();
    }

    if ((angle_relatif < PI + 0.3) && (angle_relatif > PI - 0.3)) i = -1;

    //  Calcul Consigne pour angle avec angle cible rafraichie
    if (i > 0) {
        pid_a.setCommande(angle_absolu_destination);
    } else {
        pid_a.setCommande(modulo_angle_absolu(angle_absolu_destination + PI));
    }
    angle_cons = pid_a.getConsigne();

    dist_cons = min(dist_cons, 0.4f);
    angle_cons = min(angle_cons, 0.4f);

    dist_cons = max(dist_cons, -0.4f);
    angle_cons = max(angle_cons, -0.4f);

    /*
      Initialisation des directions pour que les moteurs tournent dans
      le même sens. On inverse ensuite selon la consigne lors de l'appel
      à `limiter_consigne`.
      */
    if (i > 0) {
        dir_r = 1;
        dir_l = 0;

        cmr = dist_cons + angle_cons; // Consigne moteur droit
        cml = dist_cons - angle_cons; // Consigne moteur gauche
    }  else {
        dir_r = 0;
        dir_l = 1;

        cmr = dist_cons - angle_cons; // Consigne moteur droit
        cml = dist_cons + angle_cons; // Consigne moteur gauche
    }


    pid_v_r.setCommande(cmr);
    cmr = pid_v_r.getConsigne();

    pid_v_l.setCommande(cml);
    cml = pid_v_l.getConsigne();

    if (debug_monitor) {
        print_pos();
        printf("x:%.2f y:%.2f t:%.2f r:%.2f l:%.2f A:%.2f D:%.2f R:%.2f F:%s I:%s ",
            cible->x(), cible->y(), convert_degree(angle_absolu_destination),
            cmr, cml,
            angle_cons, dist_cons,
            convert_degree(angle_relatif),
            pid_d.actionFinished ? "T" : "F",
            // i < 0 ? "Y" : "N",
            triggered == 1 ? "Y" : "N");
    }

    limiter_consigne(&cmr, &dir_r);
    limiter_consigne(&cml, &dir_l);

    // printf("Consignes : (l : %f) (r : %f)\r\n", cmr, cml);



    if (move) {
        m_l->write(cml);
        m_r->write(cmr);
    }

    *d_r = dir_r;
    *d_l = dir_l;

}

/*
 * Tourne le robot de `angle`. Angle représente l'angle relatif,
 * il peut être positif ou négatif.
 */
void Navigateur::rotate_by(float angle)
{
    float theta = position->get_theta();
    float angle_dest = theta + angle;

    pid_a.setCommande(angle_dest);

    while (1) {

        float consigne = pid_a.getConsigne();

        consigne = min(consigne, 0.4f);

        float cmr = -consigne;
        float cml = consigne;

        int dir_l = 0;
        int dir_r = 0;

        limiter_consigne(&cmr, &dir_r);
        limiter_consigne(&cml, &dir_l);

        m_l->write(cml);
        m_r->write(cmr);

        *d_r = dir_r;
        *d_l = dir_l;

        print_pos();
    }
}

void Navigateur::print_pos()
{
    printf("\n\rx:%.2f y:%.2f t:%.2f ", position->get_x(), position->get_y(), convert_degree(position->get_theta()));
}

void Navigateur::updatePos()
{
    short vitesses[2];
    position->update(vitesses);
    pid_v_r.updateVitesse(vitesses[0]);
    pid_v_l.updateVitesse(vitesses[1]);
}
