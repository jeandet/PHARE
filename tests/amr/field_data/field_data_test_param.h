#ifndef PHARE_TESTS_AMR_FIELD_DATA_FIELD_DATA_TEST_PARAM_H
#define PHARE_TESTS_AMR_FIELD_DATA_FIELD_DATA_TEST_PARAM_H


#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <SAMRAI/pdat/CellData.h>
#include <SAMRAI/pdat/CellDataFactory.h>
#include <SAMRAI/pdat/CellVariable.h>
#include <SAMRAI/pdat/NodeData.h>
#include <SAMRAI/pdat/NodeDataFactory.h>
#include <SAMRAI/pdat/NodeVariable.h>

#include "data/field/field_data.h"
#include "data/field/field_overlap.h"
#include "data/field/field_variable.h"
#include "data/grid/gridlayout.h"
#include "data/grid/gridlayout_impl.h"
#include "utilities/point/point.h"

namespace PHARE
{
template<typename GridLayoutImpl, typename FieldImpl>
struct FieldDataTestParam
{
    FieldDataTestParam(std::string const& name, HybridQuantity::Scalar quantity,
                       SAMRAI::hier::Patch& patch_0, SAMRAI::hier::Patch& patch_1)
        : fieldDestinationVariable{name + std::string("_0"), quantity}
        , fieldSourceVariable{name + std::string("_1"), quantity}
        , fieldDestinationFactory{fieldDestinationVariable.getPatchDataFactory()}
        , fieldSourceFactory{fieldSourceVariable.getPatchDataFactory()}
        , destinationPatch{patch_0}
        , sourcePatch{patch_1}
        , destinationFieldGeometry{fieldDestinationFactory->getBoxGeometry(
              destinationPatch.getBox())}
        , sourceFieldGeometry{fieldSourceFactory->getBoxGeometry(sourcePatch.getBox())}
        , destinationFieldData{std::dynamic_pointer_cast<FieldData<GridLayoutImpl, FieldImpl>>(
              fieldDestinationFactory->allocate(destinationPatch))}
        , sourceFieldData{std::dynamic_pointer_cast<FieldData<GridLayoutImpl, FieldImpl>>(
              fieldSourceFactory->allocate(sourcePatch))}
    {
        resetValues();
    }



    /** @brief reset the values of field source and destination
     */
    void resetValues()
    {
        auto& fieldDestination = destinationFieldData->field;
        auto& fieldSource      = sourceFieldData->field;


        auto iStart
            = destinationFieldData->gridLayout.ghostStartIndex(fieldDestination, Direction::X);
        auto iEnd = destinationFieldData->gridLayout.ghostEndIndex(fieldDestination, Direction::X);

        for (auto ix = iStart; ix <= iEnd; ++ix)
        {
            fieldDestination(ix) = destinationFill(ix);
        }

        iStart = sourceFieldData->gridLayout.ghostStartIndex(fieldSource, Direction::X);
        iEnd   = sourceFieldData->gridLayout.ghostEndIndex(fieldSource, Direction::X);

        for (auto ix = iStart; ix <= iEnd; ++ix)
        {
            fieldSource(ix) = sourceFill(ix);
        }
    }




    double sourceFill(int iCell)
    {
        auto& sourceLayout = sourceFieldData->gridLayout;
        auto& sourceField  = sourceFieldData->field;

        auto origin   = sourceLayout.origin();
        auto position = sourceLayout.fieldNodeCoordinates(sourceField, origin, iCell);

        return std::cos(position[0]);
    }




    double destinationFill(int iCell)
    {
        auto& destinationLayout = destinationFieldData->gridLayout;
        auto& destinationField  = destinationFieldData->field;

        auto origin   = destinationLayout.origin();
        auto position = destinationLayout.fieldNodeCoordinates(destinationField, origin, iCell);

        return std::sin(position[0]);
    }



    FieldVariable<GridLayoutImpl, FieldImpl> fieldDestinationVariable;
    FieldVariable<GridLayoutImpl, FieldImpl> fieldSourceVariable;

    std::shared_ptr<SAMRAI::hier::PatchDataFactory> fieldDestinationFactory;
    std::shared_ptr<SAMRAI::hier::PatchDataFactory> fieldSourceFactory;

    SAMRAI::hier::Patch& destinationPatch;
    SAMRAI::hier::Patch& sourcePatch;

    std::shared_ptr<SAMRAI::hier::BoxGeometry> destinationFieldGeometry;
    std::shared_ptr<SAMRAI::hier::BoxGeometry> sourceFieldGeometry;

    std::shared_ptr<FieldData<GridLayoutImpl, FieldImpl>> destinationFieldData;
    std::shared_ptr<FieldData<GridLayoutImpl, FieldImpl>> sourceFieldData;
};

struct Patches1D
{
    SAMRAI::tbox::Dimension dim{1};
    SAMRAI::hier::BlockId blockId{0};


    SAMRAI::hier::Box destinationBoxPatchDomain{SAMRAI::hier::Index(dim, 0),
                                                SAMRAI::hier::Index(dim, 10), blockId};

    SAMRAI::hier::Box sourceBoxPatchDomain{SAMRAI::hier::Index(dim, 5),
                                           SAMRAI::hier::Index(dim, 20), blockId};

    std::shared_ptr<SAMRAI::hier::PatchDescriptor> patchDescriptor{
        std::make_shared<SAMRAI::hier::PatchDescriptor>()};


    double dx{0.01};
    double patchDestinationLower{0.};
    double patchDestinationUpper{0.1};

    double patchSourceLower{0.05};
    double patchSourceUpper{0.2};


    SAMRAI::hier::PatchGeometry::TwoDimBool touchesRegular{dim, false};

    std::shared_ptr<SAMRAI::geom::CartesianPatchGeometry> destinationPatchGeometry{
        std::make_shared<SAMRAI::geom::CartesianPatchGeometry>(
            SAMRAI::hier::IntVector::getOne(dim), touchesRegular, blockId, &dx,
            &patchDestinationLower, &patchDestinationUpper)};

    std::shared_ptr<SAMRAI::geom::CartesianPatchGeometry> sourcePatchGeometry{
        std::make_shared<SAMRAI::geom::CartesianPatchGeometry>(
            SAMRAI::hier::IntVector::getOne(dim), touchesRegular, blockId, &dx, &patchSourceLower,
            &patchSourceUpper)};



    SAMRAI::hier::Patch destinationPatch{destinationBoxPatchDomain, patchDescriptor};
    SAMRAI::hier::Patch sourcePatch{sourceBoxPatchDomain, patchDescriptor};




    Patches1D()
    {
        destinationPatch.setPatchGeometry(destinationPatchGeometry);
        sourcePatch.setPatchGeometry(sourcePatchGeometry);
    }
};


template<typename T>
struct AFieldData1DCenteredOnEx : public ::testing::Test
{
    SAMRAI::tbox::Dimension dim{1};
    SAMRAI::hier::BlockId blockId{0};

    Patches1D patch1d;

    HybridQuantity::Scalar quantity{HybridQuantity::Scalar::Ex};
    std::string name{"Ex"};

    T param{name, quantity, patch1d.destinationPatch, patch1d.sourcePatch};


    SAMRAI::hier::IntVector ghosts{SAMRAI::hier::IntVector::getZero(this->dim)};


    std::shared_ptr<SAMRAI::pdat::CellDataFactory<double>> destinationCellFactory;
    std::shared_ptr<SAMRAI::pdat::CellDataFactory<double>> sourceCellFactory;



    std::shared_ptr<SAMRAI::pdat::CellData<double>> destinationCellData;
    std::shared_ptr<SAMRAI::pdat::CellData<double>> sourceCellData;




    AFieldData1DCenteredOnEx()
    {
        ghosts[0] = param.destinationFieldData->gridLayout.nbrGhosts(
            param.destinationFieldData->gridLayout.centering(quantity)[0]);
        destinationCellFactory = std::make_shared<SAMRAI::pdat::CellDataFactory<double>>(1, ghosts);
        sourceCellFactory      = std::make_shared<SAMRAI::pdat::CellDataFactory<double>>(1, ghosts);

        destinationCellData = std::dynamic_pointer_cast<SAMRAI::pdat::CellData<double>>(
            destinationCellFactory->allocate(patch1d.destinationPatch));

        sourceCellData = std::dynamic_pointer_cast<SAMRAI::pdat::CellData<double>>(
            sourceCellFactory->allocate(patch1d.sourcePatch));
    }
};




template<typename T>
struct AFieldData1DCenteredOnEy : public ::testing::Test
{
    SAMRAI::tbox::Dimension dim{1};
    SAMRAI::hier::BlockId blockId{0};

    Patches1D patch1d;

    HybridQuantity::Scalar quantity{HybridQuantity::Scalar::Ey};
    std::string name{"Ey"};

    T param{name, quantity, patch1d.destinationPatch, patch1d.sourcePatch};


    SAMRAI::hier::IntVector ghosts{SAMRAI::hier::IntVector::getZero(this->dim)};


    std::shared_ptr<SAMRAI::pdat::NodeDataFactory<double>> destinationNodeFactory;
    std::shared_ptr<SAMRAI::pdat::NodeDataFactory<double>> sourceNodeFactory;



    std::shared_ptr<SAMRAI::pdat::NodeData<double>> destinationNodeData;
    std::shared_ptr<SAMRAI::pdat::NodeData<double>> sourceNodeData;



    AFieldData1DCenteredOnEy()
    {
        ghosts[0] = param.destinationFieldData->gridLayout.nbrGhosts(
            param.destinationFieldData->gridLayout.centering(quantity)[0]);
        destinationNodeFactory
            = std::make_shared<SAMRAI::pdat::NodeDataFactory<double>>(1, ghosts, true);
        sourceNodeFactory
            = std::make_shared<SAMRAI::pdat::NodeDataFactory<double>>(1, ghosts, true);

        destinationNodeData = std::dynamic_pointer_cast<SAMRAI::pdat::NodeData<double>>(
            destinationNodeFactory->allocate(patch1d.destinationPatch));

        sourceNodeData = std::dynamic_pointer_cast<SAMRAI::pdat::NodeData<double>>(
            sourceNodeFactory->allocate(patch1d.sourcePatch));
    }
};


// Using used later in test

using Field1D = Field<NdArrayVector1D<>, HybridQuantity::Scalar>;

using FieldDataTest1DOrder1 = FieldDataTestParam<GridLayoutImplYee<1, 1>, Field1D>;
using FieldDataTest1DOrder2 = FieldDataTestParam<GridLayoutImplYee<1, 2>, Field1D>;
using FieldDataTest1DOrder3 = FieldDataTestParam<GridLayoutImplYee<1, 3>, Field1D>;

using FieldDataTestList
    = ::testing::Types<FieldDataTest1DOrder1, FieldDataTest1DOrder2, FieldDataTest1DOrder3>;



} // namespace PHARE
#endif
