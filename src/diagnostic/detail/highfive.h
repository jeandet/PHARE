
#ifndef PHARE_DETAIL_DIAGNOSTIC_HIGHFIVE_H
#define PHARE_DETAIL_DIAGNOSTIC_HIGHFIVE_H

#include "highfive/H5DataSet.hpp"
#include "highfive/H5DataSpace.hpp"
#include "highfive/H5File.hpp"
#include "highfive/H5Easy.hpp"

#include "diagnostic/diagnostic_manager.h"
#include "diagnostic/diagnostic_dao.h"
#include "highfive_diag_writer.h"

/*TODO
  add advancement/time iteration separation for dumping
    - see "getPatchPath" - replace "/t#" with real time/id
*/

namespace PHARE::diagnostic::h5
{
using HiFile = HighFive::File;

template<typename HighFiveDiagnostic>
class ElectromagDiagnosticWriter;
template<typename HighFiveDiagnostic>
class FluidDiagnosticWriter;
template<typename HighFiveDiagnostic>
class ParticlesDiagnosticWriter;

struct HighFiveFile
{
    HiFile file_;
    PHARE::diagnostic::Mode mode_ = PHARE::diagnostic::Mode::LIGHT;

    static auto createHighFiveFile(std::string const path, unsigned flags)
    {
        return HiFile
        {
            path, flags
#if defined(H5_HAVE_PARALLEL)
                ,
                HighFive::MPIOFileDriver(MPI_COMM_WORLD, MPI_INFO_NULL)
#endif
        };
    }

    HighFiveFile(std::string const path, unsigned flags,
                 PHARE::diagnostic::Mode mode = PHARE::diagnostic::Mode::LIGHT)
        : file_{createHighFiveFile(path, flags)}
        , mode_{mode}
    {
    }
    ~HighFiveFile() {}

    HiFile& file() { return file_; }

    HighFiveFile(const HighFiveFile&)             = delete;
    HighFiveFile(const HighFiveFile&&)            = delete;
    HighFiveFile& operator&(const HighFiveFile&)  = delete;
    HighFiveFile& operator&(const HighFiveFile&&) = delete;
};



template<typename ModelView>
class HighFiveDiagnosticWriter : public PHARE::diagnostic::IDiagnosticWriter
{
public:
    using This       = HighFiveDiagnosticWriter<ModelView>;
    using GridLayout = typename ModelView::GridLayout;
    using Attributes = typename ModelView::Attributes;

    static constexpr auto dimension   = ModelView::dimension;
    static constexpr auto interpOrder = GridLayout::interp_order;

    HighFiveDiagnosticWriter(ModelView& modelView, std::string const hifivePath,
                             unsigned _flags
                             = HiFile::ReadWrite | HiFile::Create | HiFile::Truncate)
        : flags{_flags}
        , filePath_{hifivePath}
        , modelView_{modelView}
    {
    }

    static std::unique_ptr<HighFiveDiagnosticWriter> from(ModelView& modelView,
                                                          initializer::PHAREDict& dict)
    {
        std::string filePath = dict["filePath"].template to<std::string>();
        return std::move(std::make_unique<HighFiveDiagnosticWriter>(modelView, filePath));
    }

    ~HighFiveDiagnosticWriter() {}

    void dump(std::vector<DiagnosticDAO*> const&);

    template<typename String>
    auto getDiagnosticWriterForType(String& type)
    {
        return writers.at(type);
    }

    std::string fileString(std::string fileStr)
    {
        if (fileStr[0] == '/')
            fileStr = fileStr.substr(1);
        std::replace(fileStr.begin(), fileStr.end(), '/', '_');
        return fileStr + ".h5";
    }

    auto makeFile(std::string filename)
    {
        return std::make_unique<HighFiveFile>(filePath_ + "/" + filename, flags);
    }
    auto makeFile(DiagnosticDAO& diagnostic) { return makeFile(fileString(diagnostic.type)); }

    /*
     * TODO: update when time advancements are implemented.
     */
    std::string getPatchPath(std::string, int iLevel, std::string globalCoords)
    {
        return "/t#/pl" + std::to_string(iLevel) + "/p" + globalCoords;
    }

    auto& modelView() const { return modelView_; }
    const auto& patchPath() const { return patchPath_; }

    template<typename Type>
    void createDataSet(HiFile& h5, std::string const& path, size_t size)
    {
        if constexpr (std::is_same_v<Type, double>) // force doubles for floats for storage
            this->template createDatasetsPerMPI<float>(h5, path, size);
        else
            this->template createDatasetsPerMPI<Type>(h5, path, size);
    }

    template<typename Array, typename String>
    void writeDataSet(HiFile& h5, String path, Array const* const array)
    {
        h5.getDataSet(path).write(array);
        h5.flush();
    }

    // global function when all path+key are the same
    template<typename Data>
    void writeAttribute(HiFile& h5, std::string path, std::string key, Data const& value)
    {
        h5.getGroup(path)
            .template createAttribute<Data>(key, HighFive::DataSpace::From(value))
            .write(value);
        h5.flush();
    }

    template<typename Dict>
    void writeAttributeDict(HiFile& h5, Dict, std::string);

    // per MPI function where path differs per process
    template<typename Data>
    void writeAttributesPerMPI(HiFile& h5, std::string path, std::string key, Data const& value);

    template<typename VecField>
    void writeVecFieldAsDataset(HiFile& h5, std::string path, VecField& vecField)
    {
        for (auto& [id, type] : core::Components::componentMap)
            h5.getDataSet(path + "/" + id).write(vecField.getComponent(type).data());
        h5.flush();
    }

    size_t getMaxOfPerMPI(size_t localSize);

    size_t minLevel = 0, maxLevel = 10; // TODO hard-coded to be parametrized somehow
    unsigned flags;

private:
    std::string filePath_;
    std::string patchPath_; // is passed around as "virtual write()" has no parameters
    ModelView& modelView_;
    Attributes fileAttributes_;

    std::unordered_map<std::string, std::shared_ptr<Hi5DiagnosticTypeWriter<This>>> writers{
        {"fluid", make_writer<FluidDiagnosticWriter<This>>()},
        {"electromag", make_writer<ElectromagDiagnosticWriter<This>>()},
        {"particle", make_writer<ParticlesDiagnosticWriter<This>>()}};

    template<typename Writer>
    std::shared_ptr<Hi5DiagnosticTypeWriter<This>> make_writer()
    {
        return std::make_shared<Writer>(*this);
    }

    template<typename Type>
    void createDatasetsPerMPI(HiFile& h5, std::string path, size_t dataSetSize);


    void initializeDatasets_(std::vector<DiagnosticDAO*> const& diagnotics);
    void writeDatasets_(std::vector<DiagnosticDAO*> const& diagnotics);

    HighFiveDiagnosticWriter(const HighFiveDiagnosticWriter&)             = delete;
    HighFiveDiagnosticWriter(const HighFiveDiagnosticWriter&&)            = delete;
    HighFiveDiagnosticWriter& operator&(const HighFiveDiagnosticWriter&)  = delete;
    HighFiveDiagnosticWriter& operator&(const HighFiveDiagnosticWriter&&) = delete;
};



template<typename ModelView>
void HighFiveDiagnosticWriter<ModelView>::dump(std::vector<DiagnosticDAO*> const& diagnostics)
{
    fileAttributes_["dimension"]   = dimension;
    fileAttributes_["interpOrder"] = interpOrder;
    fileAttributes_["layoutType"]  = modelView().getLayoutTypeString();

    initializeDatasets_(diagnostics);
    writeDatasets_(diagnostics);
}



template<typename ModelView>
size_t HighFiveDiagnosticWriter<ModelView>::getMaxOfPerMPI(size_t localSize)
{
    int mpi_size;
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    std::vector<uint64_t> perMPI(mpi_size);
    MPI_Allgather(&localSize, 1, MPI_UINT64_T, perMPI.data(), 1, MPI_UINT64_T, MPI_COMM_WORLD);

    for (const auto size : perMPI)
        if (size > localSize)
            localSize = size;

    return localSize;
}



/*
 * Communicate all dataset paths and sizes to all MPI process to allow each to create all
 * datasets independently. This is a requirement of HDF5.
 * in the case of a disparate number of datasets per MPI process, dataSetSize may be 0
 * such that the current process creates datasets for all other processes with non-zero
 * sizes. Recommended to use similar sized paths, if possible.
 */
template<typename ModelView>
template<typename Type>
void HighFiveDiagnosticWriter<ModelView>::createDatasetsPerMPI(HiFile& h5, std::string path,
                                                               size_t dataSetSize)
{
    int mpi_size;
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    uint64_t pathSize    = path.size();
    uint64_t maxPathSize = getMaxOfPerMPI(pathSize);

    std::vector<uint64_t> datasetSizes(mpi_size);
    MPI_Allgather(&dataSetSize, 1, MPI_UINT64_T, datasetSizes.data(), 1, MPI_UINT64_T,
                  MPI_COMM_WORLD);

    std::vector<char> chars(maxPathSize * mpi_size);
    MPI_Allgather(path.c_str(), path.size(), MPI_CHAR, chars.data(), maxPathSize, MPI_CHAR,
                  MPI_COMM_WORLD);

    for (int i = 0; i < mpi_size; i++)
    {
        if (!datasetSizes[i])
            continue;
        std::string datasetPath{&chars[pathSize * i], pathSize};
        H5Easy::detail::createGroupsToDataSet(h5, datasetPath);
        h5.createDataSet<Type>(datasetPath, HighFive::DataSpace(datasetSizes[i]));
    }
}



/*
 * Communicate all attribute paths and values to all MPI process to allow each to create all
 * attributes independently. This is a requirement of HDF5.
 * in the case of a disparate number of attributes per MPI process, path may be an empty string
 * such that the current process creates attributes for all other processes with non-zero
 * sizes. Recommended to use similar sized paths, if possible. key is always assumed to the be
 * the same
 */
template<typename ModelView>
template<typename Data>
void HighFiveDiagnosticWriter<ModelView>::writeAttributesPerMPI(HiFile& h5, std::string path,
                                                                std::string key, Data const& data)
{
    int mpi_size;
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    std::vector<Data> values(mpi_size);
    if constexpr (std::is_same_v<std::string, Data>)
    {
        size_t maxMPISize = getMaxOfPerMPI(data.size());
        std::vector<char> chars(maxMPISize * mpi_size);
        MPI_Allgather(data.c_str(), data.size(), MPI_CHAR, chars.data(), maxMPISize, MPI_CHAR,
                      MPI_COMM_WORLD);
        for (int i = 0; i < mpi_size; i++)
            values[i] = std::string{&chars[maxMPISize * i], maxMPISize};
    }
    else if constexpr (std::is_same_v<double, Data>)
        MPI_Allgather(&data, 1, MPI_DOUBLE, values.data(), 1, MPI_DOUBLE, MPI_COMM_WORLD);
    else
        MPI_Allgather(&data, 1, MPI_UINT64_T, values.data(), 1, MPI_UINT64_T, MPI_COMM_WORLD);

    std::unordered_map<std::string, Data> pathValues;
    {
        uint64_t pathSize = path.size();
        auto maxPathSize  = getMaxOfPerMPI(pathSize);
        std::vector<char> chars(maxPathSize * mpi_size);
        MPI_Allgather(path.c_str(), pathSize, MPI_CHAR, chars.data(), maxPathSize, MPI_CHAR,
                      MPI_COMM_WORLD);

        for (int i = 0; i < mpi_size; i++)
        {
            auto attributePath = std::string{&chars[pathSize * i], pathSize};
            if (!pathValues.count(attributePath))
                pathValues.emplace(attributePath, values[i]);
        }
    }

    for (const auto& [keyPath, value] : pathValues)
    {
        if (!keyPath.empty())
        {
            H5Easy::detail::createGroupsToDataSet(h5, keyPath + "/dataset");
            if (!h5.getGroup(keyPath).hasAttribute(key))
                h5.getGroup(keyPath)
                    .template createAttribute<Data>(key, HighFive::DataSpace::From(value))
                    .write(value);
        }
    }
}



template<typename ModelView>
void HighFiveDiagnosticWriter<ModelView>::initializeDatasets_(
    std::vector<DiagnosticDAO*> const& diagnostics)
{
    size_t maxLocalLevel = 0;
    std::unordered_map<size_t, std::vector<std::string>> patchIDs; // level to local patches
    Attributes patchAttributes; // stores dataset info/size for synced MPI creation
    auto collectPatchAttributes = [&](GridLayout&, std::string patchID, size_t iLevel) {
        if (!patchIDs.count(iLevel))
            patchIDs.emplace(iLevel, std::vector<std::string>());

        patchIDs.at(iLevel).emplace_back(patchID);

        for (auto* diag : diagnostics)
        {
            writers.at(diag->category)->getDataSetInfo(*diag, iLevel, patchID, patchAttributes);
        }
        maxLocalLevel = iLevel;
    };

    modelView().visitHierarchy(collectPatchAttributes, minLevel, maxLevel);

    // sets empty vectors in case current process lacks patch on a level
    size_t maxMPILevel = getMaxOfPerMPI(maxLocalLevel);
    for (size_t lvl = maxLocalLevel; lvl < maxMPILevel; lvl++)
        if (!patchIDs.count(lvl))
            patchIDs.emplace(lvl, std::vector<std::string>());

    for (auto* diagnostic : diagnostics)
    {
        writers.at(diagnostic->category)
            ->initDataSets(*diagnostic, patchIDs, patchAttributes, maxMPILevel);
    }
}


template<typename ModelView>
void HighFiveDiagnosticWriter<ModelView>::writeDatasets_(
    std::vector<DiagnosticDAO*> const& diagnostics)
{
    auto writePatch = [&](GridLayout& gridLayout, std::string patchID, size_t iLevel) {
        patchPath_           = getPatchPath("time", iLevel, patchID);
        auto patchAttributes = modelView().getPatchAttributes(gridLayout);
        for (auto* diagnostic : diagnostics)
            writers.at(diagnostic->category)->write(*diagnostic, fileAttributes_, patchAttributes);
    };

    modelView().visitHierarchy(writePatch, minLevel, maxLevel);
}



/*
 * turns a dict of std::map<std::string, T> to hdf5 attributes
 */
template<typename ModelView>
template<typename Dict>
void HighFiveDiagnosticWriter<ModelView>::writeAttributeDict(HiFile& h5, Dict dict,
                                                             std::string path)
{
    using dict_map_t = typename Dict::map_t;
    auto visitor     = [&](auto&& map) {
        using Map_t = std::decay_t<decltype(map)>;
        if constexpr (std::is_same_v<Map_t, dict_map_t>)
            for (auto& pair : map)
                std::visit(
                    [&](auto&& val) {
                        using Val = std::decay_t<decltype(val)>;
                        if constexpr (core::is_dict_leaf<Val, Dict>::value)
                            writeAttributesPerMPI(h5, path, pair.first, val);
                        else
                            throw std::runtime_error(std::string("Expecting Writable value got ")
                                                     + typeid(Map_t).name());
                    },
                    pair.second.get()->data);
        else
            /* static_assert fails without if ! constexpr all possible types
             * regardless of what it actually is.
             */
            throw std::runtime_error(std::string("Expecting map<string, T> got ")
                                     + typeid(Map_t).name());
    };
    std::visit(visitor, dict.data);
}



} /* namespace PHARE::diagnostic::h5 */

#endif /* PHARE_DETAIL_DIAGNOSTIC_HIGHFIVE_H */