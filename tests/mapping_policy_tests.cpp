#include "si/mapping_policy.hpp"
#include <iostream>

using namespace si;
static void expect(bool condition, const char *message) {
  if (!condition) throw Error(std::string("TEST FAILED: ") + message);
}

int main() {
  try {
    Channel twoEnded;
    twoEnded.nearP = 0;
    twoEnded.farP = 1;
    Channel reflectionOnly;
    reflectionOnly.nearP = 0;

    Metadata plain;
    expect(requiresManualDirectionConfirmation(plain, {twoEnded}),
           "label-only two-ended mapping requires Near/Far confirmation");
    expect(!requiresManualDirectionConfirmation(plain, {reflectionOnly}),
           "single-port reflection mapping may be confirmed without direction");

    Metadata mixed;
    mixed.mixedOrder = {"d1,2", "d3,4"};
    expect(requiresManualDirectionConfirmation(mixed, {twoEnded}),
           "mixed-mode P/N order does not prove Near/Far direction");
    expect(requiresManualDirectionConfirmation(mixed, {reflectionOnly}),
           "mixed-mode metadata remains review-required without direction evidence");

    Metadata powerSI;
    powerSI.differentialHints = {"validated explicit endpoint metadata"};
    expect(!requiresManualDirectionConfirmation(powerSI, {twoEnded}),
           "explicit PowerSI endpoint evidence may bypass manual direction review");

    std::cout << "mapping confirmation policy PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
