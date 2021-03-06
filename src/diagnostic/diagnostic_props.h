#ifndef DIAGNOSTIC_DAO_H
#define DIAGNOSTIC_DAO_H

#include <cstddef>
#include <string>
#include <vector>


namespace PHARE::diagnostic
{
struct DiagnosticProperties
{
    std::vector<double> writeTimestamps, computeTimestamps;
    std::string type, quantity;
};

} // namespace PHARE::diagnostic

#endif // DIAGNOSTIC_DAO_H
