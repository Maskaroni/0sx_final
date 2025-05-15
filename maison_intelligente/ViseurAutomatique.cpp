#include <Arduino.h>
#include <AccelStepper.h>
#include "ViseurAutomatique.h"

#define MOTOR_INTERFACE_TYPE 4
const int zero = 0;

ViseurAutomatique::ViseurAutomatique(int p1, int p2, int p3, int p4, float* distanceRef) {
  _stepper = AccelStepper(MOTOR_INTERFACE_TYPE, p1, p3, p2, p4);
  _stepper.setMaxSpeed(500);
  _stepper.setAcceleration(150);
	_stepper.setSpeed(150);
  _distance = distanceRef;
}

void ViseurAutomatique::update() {
  _currentTime = millis();

  switch (_etat) {
    case INACTIF:
      _inactifState();
      break;
    case SUIVI:
      _suiviState();
      break;
    case REPOS:
      _reposState();
      break;
  }

  _stepper.run();
}

void ViseurAutomatique::setAngleMin(float angle) {
  _angleMin = angle;
}

void ViseurAutomatique::setAngleMax(float angle) {
  _angleMax = angle;
}

void ViseurAutomatique::setPasParTour(int steps) {
  _stepsPerRev = steps;
}

void ViseurAutomatique::setDistanceMinSuivi(float distanceMin) {
  _distanceMinSuivi = distanceMin;
}

void ViseurAutomatique::setDistanceMaxSuivi(float distanceMax) {
  _distanceMaxSuivi = distanceMax;
}

float ViseurAutomatique::getAngle() const {
  float angle = (map(*_distance, _distanceMinSuivi, _distanceMaxSuivi, _angleMin, _angleMax));
  if (angle > _angleMax) {
    return _angleMax;
  }
  else if  (angle < _angleMin) {
    return _angleMin;
  }
  else {
    return angle;
  }
}

void ViseurAutomatique::activer() {
  _etat = REPOS;
}

void ViseurAutomatique::desactiver() {
  _etat = INACTIF;
}

const char* ViseurAutomatique::getEtatTexte() const {
  switch (_etat) {
    case 0:
      return "INACTIF";
      break;
    case 1:
      return "SUIVI";
      break;
    case 2:
      return "REPOS";
      break;
    default:
      return "Erreur";
      break;
  }
}

void ViseurAutomatique::_inactifState() {
  _stepper.disableOutputs();
}

void ViseurAutomatique::_suiviState() {
  _etat = (*_distance < _distanceMinSuivi || *_distance > _distanceMaxSuivi) ? REPOS : SUIVI;
  _stepper.moveTo(_angleEnSteps((int)getAngle()));
}

void ViseurAutomatique::_reposState() {
  _etat = (*_distance < _distanceMinSuivi || *_distance > _distanceMaxSuivi) ? REPOS : SUIVI;
}


long ViseurAutomatique::_angleEnSteps(float angle) const {
  return (map(angle, _angleMin, _angleMax, (-1 * (_stepsPerRev/4)), (_stepsPerRev/4)));
}