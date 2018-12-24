#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <cstdlib>

#include <stdexcept>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <deque>
#include <set>
#include <map>

#include <BWAPI.h>

#include "Config.h"
#include "Logger.h"
#include "UABAssert.h"

BWAPI::AIModule* __NewAIModule();

// 2D vectors with basic vector arithmetic.
struct V2 {
  double x, y;

  V2() { }

  V2(const double x, const double y) : x(x), y(y) { }

  V2(const BWAPI::Position& p) : x(p.x), y(p.y) { }

  operator BWAPI::Position() const { return BWAPI::Position(static_cast<int>(x), static_cast<int>(y)); }

  V2 operator +(const V2& v) const { return V2(x + v.x, y + v.y); }
  V2 operator -(const V2& v) const { return V2(x - v.x, y - v.y); }
  V2 operator *(double s) const { return V2(x * s, y * s); }
  V2 operator /(double s) const { return V2(x / s, y / s); }

  double Dot(const V2& v) const { return x * v.x + y * v.y; }
  double LengthSq() const { return x * x + y * y; }
  double Length() const { return sqrt(LengthSq()); }

  // Find the direction: The vector of length 1 in the same direction.
  // The length of the original vector had better not be zero!
  V2 Normalize() const { return *this / Length(); }

  // This uses trig, so it is probably slower.
  void Rotate(double angle) {
    angle = angle * M_PI / 180.0; // convert degrees to radians
    *this = V2(x * cos(angle) - y * sin(angle), y * cos(angle) + x * sin(angle));
  }
};

struct Rect {
  int x_, y_;
  int height_, width_;
};

double Ucb1Bound(int tries, int total);
double Ucb1Bound(double tries, double total);

// Used to return a reference to an empty set of units when a data structure
// doesn't have an entry giving another set.
const static BWAPI::Unitset kEmptyUnitSet;

BWAPI::Unitset Intersection(const BWAPI::Unitset& a, const BWAPI::Unitset& b);

int GetIntFromString(const std::string& s);
std::string TrimRaceName(const std::string& s);
char RaceChar(BWAPI::Race race);
std::string NiceMacroActName(const std::string& s);
std::string UnitTypeName(BWAPI::UnitType type);

// Short color codes for drawing text on the screen.
// The dim colors can be hard to read, but are useful occasionally.
const char kYellow = '\x03';
const char kWhite = '\x04';
const char kDarkRed = '\x06'; // dim
const char kGreen = '\x07';
const char kRed = '\x08';
const char kPurple = '\x10'; // dim
const char kOrange = '\x11';
const char kGray = '\x1E'; // dim
const char kCyan = '\x1F';

void ClipToMap(BWAPI::Position& pos);
BWAPI::Position DistanceAndDirection(const BWAPI::Position& a, const BWAPI::Position& b, int distance);
double ApproachSpeed(const BWAPI::Position& pos, BWAPI::Unit u);
BWAPI::Position CenterOfUnitset(const BWAPI::Unitset units);
BWAPI::Position PredictMovement(BWAPI::Unit unit, int frames);
bool CanCatchUnit(BWAPI::Unit chaser, BWAPI::Unit runaway);
