#include "vertex.h"
#include "global.h"
#include "utility/abort.h"
#include "utility/fmt/format.h"
#include "utility/utility.h"
#include <cmath>
#include <iostream>

using namespace diag;
using namespace std;

extern parameter Para;

// double sum2(const momentum &Mom) {
//   double Sum2 = 0.0;
//   for (int i = 0; i < D; i++)
//     Sum2 += Mom[i] * Mom[i];
//   return Sum2;
// }

// double norm2(const momentum &Mom) { return sqrt(sum2(Mom)); }

double bose::Interaction(double Tau, const momentum &Mom, int VerType) {
  if (Para.InterType == YOKAWAR) {
    // yokawar interaction
    if (VerType >= 0) {
      double interaction = 8.0 * PI / (Mom.squaredNorm() + Para.Mass2);
      if (VerType > 0) {
        // the interaction contains counter-terms
        interaction *=
            pow(Para.Mass2 / (Mom.squaredNorm() + Para.Mass2), VerType);
        interaction *= pow(-1, VerType);
      }
      return interaction;
    } else if (VerType == -1) {
      return 1.0;
    } else {
      ABORT("VerType can not be " << VerType);
    }
  } else {
    ABORT("Interaction Type " << VerType << " is not implemented!");
  }
}

fermi::fermi() {
  UpperBound = 5.0 * Para.Ef;
  LowerBound = 0.0;
  DeltaK = UpperBound / MAXSIGMABIN;
  UpperBound2 = 1.2 * Para.Ef;
  LowerBound2 = 0.8 * Para.Ef;
  DeltaK2 = UpperBound2 / MAXSIGMABIN;
  if (Para.SelfEnergyType == FOCK)
    BuildFockSigma();
}

double fermi::Fock(double k) {
  // warning: this function only works for T=0!!!!
  double l = sqrt(Para.Mass2);
  double kF = Para.Kf;
  double fock = 1.0 + l / kF * atan((k - kF) / l);
  fock -= l / kF * atan((k + kF) / l);
  fock -= (l * l - k * k + kF * kF) / 4.0 / k / kF *
          log((l * l + (k - kF) * (k - kF)) / (l * l + (k + kF) * (k + kF)));
  fock *= (-2.0 * kF) / PI;

  double shift = 1.0 - l / kF * atan(2.0 * kF / l);
  shift -= l * l / 4.0 / kF / kF * log(l * l / (l * l + 4.0 * kF * kF));
  shift *= (-2.0 * kF) / PI;

  return fock - shift;
}

double fermi::BuildFockSigma() {
  ASSERT_ALLWAYS(D == 3, "The Fock self energy is for 3D!");
  double fock, k;
  for (int i = 0; i < MAXSIGMABIN; ++i) {
    // k: (0^+, UpBound^-)
    // i=0 ==> k==0.5*DeltaK
    // i=MAXSIGMABIN-1 ==> k==(MAXSIGMABIN-0.5)*DeltaK
    k = (i + 0.5) * DeltaK + LowerBound;
    Sigma[i] = Fock(k);
    if (i > 0 && k <= LowerBound2 && k >= UpperBound2) {
      ASSERT_ALLWAYS(
          Equal(Sigma[i - 1], Sigma[i], 5.0e-5),
          fmt::format("Fock are not accurate enough! At k={0}: {1} vs {2}\n", k,
                      Sigma[i - 1], Sigma[i]));
    }
    // cout << k << " : " << Sigma[i] << " vs " << Fock(k) << endl;
  }

  for (int i = 0; i < MAXSIGMABIN; ++i) {
    // k: (0^+, UpBound^-)
    // i=0 ==> k==0.5*DeltaK
    // i=MAXSIGMABIN-1 ==> k==(MAXSIGMABIN-0.5)*DeltaK
    k = (i + 0.5) * DeltaK2 + LowerBound2;
    Sigma2[i] = Fock(k);
    if (i > 0) {
      ASSERT_ALLWAYS(Equal(Sigma2[i - 1], Sigma2[i], 5.0e-5),
                     fmt::format("The 2rd level Fock are not accurate enough!"
                                 "level! At k={0}: {1} vs {2}\n",
                                 k, Sigma2[i - 1], Sigma2[i]));
    }
    // cout << k << " : " << Sigma[i] << " vs " << Fock(k) << endl;
  }
};

double fermi::FockSigma(double k) {
  // double k = Mom.norm(); // bare propagator
  double fock;
  if (k >= LowerBound2 && k < UpperBound2) {
    int i = (k - LowerBound2) / DeltaK2;
    fock = Sigma2[i];
  } else if ((k >= LowerBound && k < LowerBound2) ||
             (k >= UpperBound2 && k < UpperBound)) {
    int i = (k - LowerBound) / DeltaK;
    fock = Sigma[i];
  } else {
    fock = Fock(k);
  }
  // ASSERT_ALLWAYS(
  //     Equal(fock, Fock(k), 5.0e-5),
  //     fmt::format("Fock are not accurate enough! At k={0}: {1} vs {2}\n",
  //     k,
  //                 fock, Fock(k)));
  return fock + k * k;
}

double fermi::PhyGreen(double Tau, const momentum &Mom) {
  // if tau is exactly zero, set tau=0^-
  double green, Ek, KK;
  if (Tau == 0.0) {
    return EPS;
  }

  double s = 1.0;
  if (Tau < 0.0) {
    Tau += Para.Beta;
    s = -s;
  } else if (Tau >= Para.Beta) {
    Tau -= Para.Beta;
    s = -s;
  }

  KK = Mom.norm();

  //// enforce an UV cutoff for the Green's function ////////
  if (KK > Para.UVScale) {
    return 0.0;
  }

  if (Para.SelfEnergyType == BARE)
    Ek = KK * KK; // bare propagator
  else if (Para.SelfEnergyType == FOCK)
    Ek = FockSigma(KK); // Fock diagram dressed propagator
  else
    ABORT("Green function is not implemented!");

  double x = Para.Beta * (Ek - Para.Mu) / 2.0;
  double y = 2.0 * Tau / Para.Beta - 1.0;
  if (x > 100.0)
    green = exp(-x * (y + 1.0));
  else if (x < -100.0)
    green = exp(x * (1.0 - y));
  else
    green = exp(-x * y) / (2.0 * cosh(x));

  green *= s;

  // if (Debug) { //   cout << "Tau=" << Tau << endl;
  //   cout << "Counter" << Para.Counter << endl;
  //   cout << "Tau=" << Tau << endl;
  //   cout << "Mom=(" << Mom[0] << ", " << Mom[1] << ")" << endl;
  //   cout << "Mom2=" << Mom[0] * Mom[0] + Mom[1] * Mom[1] << endl;
  //   cout << "Ek=" << Ek << endl;
  //   cout << "x=" << x << endl;
  //   cout << "y=" << y << endl;
  //   cout << "Green=" << green << endl << endl;
  // }

  // cout << "x: " << x << ", y: " << y << ", G: " << green << endl;
  // cout << "G: " << green << endl;

  if (std::isnan(green))
    ABORT("Step:" << Para.Counter << ", Green is too large! Tau=" << Tau
                  << ", Ek=" << Ek << ", Green=" << green << ", Mom"
                  << ToString(Mom));
  return green;
}

double fermi::PhyGreenDerivative(double Tau, const momentum &Mom) {
  // if tau is exactly zero, set tau=0^-
  double green, Ek, KK;
  if (Tau == 0.0) {
    return EPS;
  }

  double s = 1.0;
  if (Tau < 0.0) {
    Tau += Para.Beta;
    s = -s;
  } else if (Tau >= Para.Beta) {
    Tau -= Para.Beta;
    s = -s;
  }

  KK = Mom.norm();

  //// enforce an UV cutoff for the Green's function ////////
  if (KK > Para.UVScale) {
    return 0.0;
  }

  if (Para.SelfEnergyType == BARE)
    Ek = KK * KK; // bare propagator
  else
    ABORT("Green function is not implemented!");

  double Factor = 0.0;

  double x = Para.Beta * (Ek - Para.Mu) / 2.0;
  double y = 2.0 * Tau / Para.Beta - 1.0;
  if (x > 100.0) {
    green = exp(-x * (y + 1.0));
    Factor = -Tau;
  } else if (x < -100.0) {
    green = exp(x * (1.0 - y));
    Factor = Para.Beta - Tau;
  } else {
    green = exp(-x * y) / (2.0 * cosh(x));
    Factor = Para.Beta / (1.0 + exp(2.0 * x)) - Tau;
  }

  green *= s;
  green *= Factor * 2.0 * KK;

  // if (Debug) { //   cout << "Tau=" << Tau << endl;
  //   cout << "Counter" << Para.Counter << endl;
  //   cout << "Tau=" << Tau << endl;
  //   cout << "Mom=(" << Mom[0] << ", " << Mom[1] << ")" << endl;
  //   cout << "Mom2=" << Mom[0] * Mom[0] + Mom[1] * Mom[1] << endl;
  //   cout << "Ek=" << Ek << endl;
  //   cout << "x=" << x << endl;
  //   cout << "y=" << y << endl;
  //   cout << "Green=" << green << endl << endl;
  // }

  // cout << "x: " << x << ", y: " << y << ", G: " << green << endl;
  // cout << "G: " << green << endl;

  if (std::isnan(green))
    ABORT("Step:" << Para.Counter << ", Green is too large! Tau=" << Tau
                  << ", Ek=" << Ek << ", Green=" << green << ", Mom"
                  << ToString(Mom));
  return green;
}

double fermi::Green(double Tau, const momentum &Mom, spin Spin, int GType) {
  double green;
  if (GType == 0) {
    green = PhyGreen(Tau, Mom);
  } else if (GType == 1) {
    // k-derivative of green's function
    green = PhyGreenDerivative(Tau, Mom);
  } else if (GType == -2) {
    // equal time green's function
    green = PhyGreen(-1.0e-10, Mom);
  } else if (GType == -1) {
    // green = PhyGreen(Tau, Mom);
    green = 1.0;
  } else {
    ABORT("GType " << GType << " has not yet been implemented!");
    // return FakeGreen(Tau, Mom);
  }
  return green;
}

verfunc::verfunc() {
  // test angle utility

  // TODO: implement D=3
  if (D == 3)
    return;

  _TestAngle2D();
  _TestAngleIndex();

  // initialize UV ver4 table
  if (Para.InterType == PWAVE_ON_EF) {
    momentum KInL = {1.0, 0.0};
    for (int scale = 0; scale < ScaleBinSize; scale++)
      for (int inin = 0; inin < InInAngBinSize; ++inin)
        for (int inout = 0; inout < InOutAngBinSize; ++inout) {
          double AngleInIn = Index2Angle(inin, InInAngBinSize);
          double AngleInOut = Index2Angle(inout, InOutAngBinSize);
          momentum KInR = {cos(AngleInIn), sin(AngleInIn)};
          momentum KOutL = {cos(AngleInOut), sin(AngleInOut)};
          momentum KOutR = KInL + KInR - KOutL;

          ASSERT_ALLWAYS(abs(Angle2D(KInL, KInR) - AngleInIn) < 1.e-2,
                         fmt::format("Angle different! {0} vs {1} ",
                                     Angle2D(KInL, KInR), AngleInIn));
          ASSERT_ALLWAYS(abs(Angle2D(KInL, KOutL) - AngleInOut) < 1.e-2,
                         fmt::format("Angle different! {0} vs {1} ",
                                     Angle2D(KInL, KOutL), AngleInOut));

          Ver4AtUV[scale][inin][inout] =
              Para.UVCoupling * (KInL - KInR).dot(KOutL - KOutR);
        }
  }
}

void verfunc::Vertex4(double &Direct, double &Exchange, const momentum &InL,
                      const momentum &InR, const momentum &OutL,
                      const momentum &OutR, int Ver4TypeDirect,
                      int Ver4TypeExchange, int Scale) {

  /**************   Yokawar Interaction ************************/
  if (Para.InterType == YOKAWAR) {
    Direct = 8.0 * PI / ((OutL - InL).squaredNorm() + Para.Mass2);
    Exchange = 8.0 * PI / ((OutR - InL).squaredNorm() + Para.Mass2);
  } else if (Para.InterType == PWAVE_ON_EF) {

    if (Ver4TypeDirect == 0 && Ver4TypeExchange == 0) {
      double AngleInIn = Angle2D(InL, InR);
      double AngleInOut = Angle2D(InL, OutL);
      int InInIndex = Angle2Index(AngleInIn, InInAngBinSize);
      int InOutIndex = Angle2Index(AngleInOut, InOutAngBinSize);
      Direct = Ver4AtUV[Scale][InInIndex][InOutIndex];

      AngleInOut = Angle2D(InL, OutR);
      InOutIndex = Angle2Index(AngleInOut, InOutAngBinSize);
      Exchange = Ver4AtUV[Scale][InInIndex][InOutIndex];
      Exchange *= -1.0; // spin factor already carries a minus sign
      // Note that InL/|InL|, InR/|InR|, OutL/|OutL|, OutR/|OutR| do not obey
      // conservation law!
      // Therefore, direct does not equal to exchange
    } else if (Ver4TypeDirect == -1 && Ver4TypeExchange == -1) {
      // double KScale = Para.Scales[Scale];
      // make the total weight scale invariant
      Direct = Para.UVCoupling * (Para.Scales[Scale] - Para.Scales[Scale + 1]);
      Exchange = Direct;
    } else
      ABORT("Ver4Type is only implemented for 0!");
  } else {
    ABORT("Interaction Type is not implemented!");
  }

  /**************   Generic Interaction ************************/
}

double verfunc::Angle2D(const momentum &K1, const momentum &K2) {
  // Returns the angle in radians between vectors 'K1' and 'K2'
  double dotp = K1.dot(K2);
  double det = K1[0] * K2[1] - K1[1] * K2[0];
  double Angle2D = atan2(det, dotp);
  if (Angle2D < 0)
    Angle2D += 2.0 * PI;
  return Angle2D;
}

double verfunc::Index2Angle(const int &Index, const int &AngleNum) {
  // Map index [0...AngleNum-1] to the theta range [0.0, 2*pi)
  return Index * 2.0 * PI / AngleNum;
}

int verfunc::Angle2Index(const double &Angle, const int &AngleNum) {
  // Map theta range  [0.0, 2*pi) to index [0...AngleNum-1]
  double dAngle = 2.0 * PI / AngleNum;
  if (Angle >= 2.0 * PI - dAngle / 2.0 || Angle < dAngle / 2.0)
    return 0;
  else
    return int(Angle / dAngle + 0.5);
}

void verfunc::_TestAngle2D() {
  // Test Angle functions
  momentum K1 = {1.0, 0.0};
  momentum K2 = {1.0, 0.0};

  ASSERT_ALLWAYS(
      abs(Angle2D(K1, K2)) < 1.e-7,
      fmt::format("Angle between K1 and K2 are not zero! It is {:.13f}",
                  Angle2D(K1, K2)));

  K1 = {1.0, 0.0};
  K2 = {0.0, 1.0};
  ASSERT_ALLWAYS(
      abs(Angle2D(K1, K2) - PI / 2.0) < 1.e-7,
      fmt::format("Angle between K1 and K2 are not Pi! Instead, it is {:.13f}",
                  Angle2D(K1, K2)));

  K1 = {1.0, 0.0};
  K2 = {-1.0, 0.0};
  ASSERT_ALLWAYS(
      abs(Angle2D(K1, K2) - PI) < 1.e-7,
      fmt::format("Angle between K1 and K2 are not Pi! Instead, it is {:.13f}",
                  Angle2D(K1, K2)));

  K1 = {1.0, 0.0};
  K2 = {1.0, -EPS};
  ASSERT_ALLWAYS(
      abs(Angle2D(K1, K2) - 2.0 * PI) < 1.e-7,
      fmt::format("Angle between K1 and K2 are not 2.0*Pi! It is {:.13f}",
                  Angle2D(K1, K2)));
}

void verfunc::_TestAngleIndex() {
  // Test Angle functions
  int AngleNum = 64;
  ASSERT_ALLWAYS(abs(Index2Angle(0, AngleNum) - 0.0) < 1.0e-10,
                 "Angle for index 0 should be zero!");

  ASSERT_ALLWAYS(abs(Index2Angle(AngleNum - 1, AngleNum) -
                     (2.0 * PI * (1.0 - 1.0 / AngleNum))) < 1.0e-10,
                 "Angle for index AngleNum should be 2.0*pi-0^+!");

  ASSERT_ALLWAYS(Angle2Index(0.0, AngleNum) == 0,
                 "Angle zero should have index 0!");
  ASSERT_ALLWAYS(
      Angle2Index(2.0 * PI * (1.0 - 0.5 / AngleNum) + EPS, AngleNum) == 0,
      "Angle 2*pi-pi/AngleNum should have index 1!");

  ASSERT_ALLWAYS(Angle2Index(2.0 * PI * (1.0 - 0.5 / AngleNum) - EPS,
                             AngleNum) == AngleNum - 1,
                 "Angle 2*pi-pi/AngleNum-0^+ should have index AngleNum!");
}