#include <iostream>
#include <cmath>
using namespace std;

int main() {

    // Input data
    double gamma = 0;
    double z = 0;
    double beta = 0;
    double c = 0;
    double phi = 0;
    double u = 0;

    beta = beta * M_PI / 180;
    phi = phi * M_PI / 180;

    // DF
    double DF = gamma * z * sin(beta) * cos(beta);

    // RF
    double RF = c + (gamma * z * pow(cos(beta), 2) - u) * tan(phi);

    // Ratio
    double result = DF / RF;

    return 0;
}
