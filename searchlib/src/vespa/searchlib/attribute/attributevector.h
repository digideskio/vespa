// Copyright 2016 Yahoo Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include "address_space_usage.h"
#include "iattributesavetarget.h"
#include <vespa/document/update/arithmeticvalueupdate.h>
#include <vespa/document/update/mapvalueupdate.h>
#include <vespa/fastlib/io/bufferedfile.h>
#include <vespa/fastlib/text/normwordfolder.h>
#include <vespa/fastos/fastos.h>
#include <vespa/searchcommon/attribute/config.h>
#include <vespa/searchcommon/attribute/iattributevector.h>
#include <vespa/searchcommon/attribute/status.h>
#include <vespa/searchcommon/common/undefinedvalues.h>
#include <vespa/searchlib/attribute/changevector.h>
#include <vespa/searchlib/common/address_space.h>
#include <vespa/searchlib/common/bitvector.h>
#include <vespa/searchlib/common/range.h>
#include <vespa/searchlib/common/rcuvector.h>
#include <vespa/searchlib/query/query.h>
#include <vespa/searchlib/queryeval/searchiterator.h>
#include <vespa/searchlib/util/fileutil.h>
#include <vespa/vespalib/objects/identifiable.h>
#include <vespa/vespalib/stllike/asciistream.h>
#include <vespa/vespalib/util/rwlock.h>
#include <vespa/vespalib/util/sync.h>
#include <cmath>
#include <mutex>
#include <shared_mutex>
#include <string>

using document::ArithmeticValueUpdate;
using document::MapValueUpdate;
using document::FieldValue;

namespace vespalib
{

class GenericHeader;

}

 
namespace search {

template <typename T> class ComponentGuard;
class AttributeReadGuard;
class AttributeWriteGuard;
class AttributeSaver;
class EnumStoreBase;

class IDocumentWeightAttribute;

namespace fef {
class TermFieldMatchData;
}

namespace attribute
{

class IPostingListSearchContext;

class IPostingListAttributeBase;

class Interlock;
class InterlockGuard;
class MultiValueMapping2Base;

}

using search::attribute::WeightedType;
using search::attribute::Status;

template <typename T>
class UnWeightedType
{
public:
    UnWeightedType() : _value(T()) { }
    UnWeightedType(T v) : _value(v) { }
    const T & getValue() const { return _value; }
    void setValue(const T & v) { _value = v; }
    int32_t getWeight()  const { return 1; }
    void setWeight(int32_t w)  { (void) w; }

    bool operator==(const UnWeightedType<T> & rhs) const {
        return _value == rhs._value;
    }

    friend vespalib::asciistream &
    operator << (vespalib::asciistream & os, const UnWeightedType & v) {
        return os << "(" << v._value << ", 1)";
    }
private:
    T       _value;
};

class IExtendAttribute
{
public:
    virtual bool add(int64_t, int32_t = 1) { return false; }
    virtual bool add(double, int32_t = 1) { return false; }
    virtual bool add(const char *, int32_t = 1) { return false; }
    
    virtual ~IExtendAttribute() {}
};

class AttributeVector : public vespalib::Identifiable,
                        public attribute::IAttributeVector
{
protected:
    typedef search::attribute::Config Config;
    typedef search::attribute::CollectionType CollectionType;
    typedef search::attribute::BasicType BasicType;
public:
    typedef std::shared_ptr<AttributeVector> SP;
    class BaseName : public vespalib::string
    {
    public:
        typedef vespalib::string string;
        BaseName(const vespalib::stringref &s)
            : string(s),
              _name(createAttributeName(s))
        {
        }
        BaseName & operator = (const vespalib::stringref & s) {
            BaseName n(s);
            std::swap(*this, n);
            return *this;
        }

        BaseName(const vespalib::stringref &base,
                 const vespalib::stringref &snap,
                 const vespalib::stringref &name);

        string getIndexName() const;
        string getSnapshotName() const;
        const string & getAttributeName() const { return _name; }
        string getDirName() const;
    private:
        static string createAttributeName(const vespalib::stringref & s);
        string _name;
    };

    class ReaderBase
    {
    public:
        ReaderBase(AttributeVector & attr);

        virtual ~ReaderBase();

        void rewind();

        bool hasWeight() const {
            return _weightFile.get() && _weightFile->IsOpened();
        }

        bool hasIdx() const {
            return _idxFile.get() && _idxFile->IsOpened();
        }

        bool hasData() const {
            return _datFile.get() && _datFile->IsOpened();
        }

        bool hasUData() const {
            return _udatFile.get() && _udatFile->IsOpened();
        }

        uint32_t getNumIdx() const {
            return (_idxFileSize - _idxHeaderLen) /sizeof(uint32_t);
        }

        size_t getEnumCount(void) const {
            size_t dataSize(_datFileSize - _datHeaderLen);
            assert((dataSize % sizeof(uint32_t)) == 0);
            return dataSize / sizeof(uint32_t);
        }

        static bool
        extractFileSize(const vespalib::GenericHeader &header,
                        FastOS_FileInterface &file, uint64_t &fileSize);

        size_t getNumValues();
        int32_t getNextWeight() { return _weightReader.readHostOrder(); }
        uint32_t getNextEnum(void) { return _enumReader.readHostOrder(); }
        bool getEnumerated(void) const { return _enumerated; }
        uint32_t getNextValueCount();
        int64_t getCreateSerialNum(void) const { return _createSerialNum; }
        bool getHasLoadData(void) const { return _hasLoadData; }
        uint32_t getVersion() const { return _version; }
        uint32_t getDocIdLimit() const { return _docIdLimit; }
        const vespalib::GenericHeader &getDatHeader() const {
            return _datHeader;
        }
    protected:
        std::unique_ptr<Fast_BufferedFile>  _datFile;
    private:
        std::unique_ptr<Fast_BufferedFile>  _weightFile;
        std::unique_ptr<Fast_BufferedFile>  _idxFile;
        std::unique_ptr<Fast_BufferedFile>  _udatFile;
        FileReader<int32_t>   _weightReader;
        FileReader<uint32_t>  _idxReader;
        FileReader<uint32_t>  _enumReader;
        uint32_t              _currIdx;
        uint32_t	      _datHeaderLen;
        uint32_t              _idxHeaderLen;
        uint32_t              _weightHeaderLen;
        uint32_t              _udatHeaderLen;
        uint64_t              _createSerialNum;
        size_t                _fixedWidth;
        bool                  _enumerated;
        bool                  _hasLoadData;
        uint32_t              _version;
        uint32_t              _docIdLimit;
        vespalib::FileHeader  _datHeader;
        uint64_t              _datFileSize;
        uint64_t              _idxFileSize;
    protected:
        size_t getDataCountHelper(size_t elemSize) const {
            size_t dataSize(_datFileSize - _datHeaderLen);
            return dataSize / elemSize;
        }
    };

    template <typename T>
    class PrimitiveReader : public ReaderBase
    {
    public:
        PrimitiveReader(AttributeVector &attr)
            : ReaderBase(attr),
              _datReader(*_datFile)
        {
        }

        virtual ~PrimitiveReader() { }
        T getNextData() { return _datReader.readHostOrder(); }
        size_t getDataCount() const { return getDataCountHelper(sizeof(T)); }
    private:
        FileReader<T> _datReader;
    };

    using GenerationHandler = vespalib::GenerationHandler;
    using GenerationHolder = vespalib::GenerationHolder;
    typedef GenerationHandler::generation_t generation_t;

    virtual ~AttributeVector();
protected:
    /**
     * Will update statistics by calling onUpdateStat if necessary.
     */
    void updateStat(bool forceUpdate);

    void
    updateStatistics(uint64_t numValues,
                     uint64_t numUniqueValue,
                     uint64_t allocated,
                     uint64_t used,
                     uint64_t dead,
                     uint64_t onHold);

    void performCompactionWarning();

    void getByType(DocId doc, const char *&v) const {
        char tmp[1024]; v = getString(doc, tmp, sizeof(tmp));
    }

    void getByType(DocId doc, vespalib::string &v) const {
        char tmp[1024]; v = getString(doc, tmp, sizeof(tmp));
    }

    void getByType(DocId doc, largeint_t & v) const {
        v = getInt(doc);
    }

    void getByType(DocId doc, double &v) const {
        v = getFloat(doc);
    }

    uint32_t getByType(DocId doc, const char **v, uint32_t sz) const {
        return get(doc, v, sz);
    }

    uint32_t getByType(DocId doc, vespalib::string *v, uint32_t sz) const {
        return get(doc, v, sz);
    }

    uint32_t getByType(DocId doc, largeint_t * v, uint32_t sz) const {
        return get(doc, v, sz);
    }

    uint32_t getByType(DocId doc, double *v, uint32_t sz) const {
        return get(doc, v, sz);
    }


    AttributeVector(const vespalib::stringref &baseFileName, const Config & c);

    void checkSetMaxValueCount(int index) {
        _highestValueCount = std::max(index, _highestValueCount);
    }

    void setEnumMax(uint32_t e)          { _enumMax = e; setEnum(); }
    void setEnum(bool hasEnum_=true)     { _hasEnum = hasEnum_; }
    void setSortedEnum(bool sorted=true) { _hasSortedEnum = sorted; }
    void setNumDocs(uint32_t n)          { _status.setNumDocs(n); }
    void incNumDocs()                    { _status.incNumDocs(); }

    std::unique_ptr<Fast_BufferedFile> openDAT();

    std::unique_ptr<Fast_BufferedFile> openIDX();

    std::unique_ptr<Fast_BufferedFile> openWeight();

    std::unique_ptr<Fast_BufferedFile> openUDAT();

    FileUtil::LoadedBuffer::UP loadDAT();

    FileUtil::LoadedBuffer::UP loadIDX();

    FileUtil::LoadedBuffer::UP loadWeight();

    FileUtil::LoadedBuffer::UP loadUDAT();

    class ValueModifier
    {
    public:
        ValueModifier(AttributeVector &attr);
        ValueModifier(const ValueModifier &rhs);
        ~ValueModifier();
    private:
        AttributeVector * stealAttr() const {
            AttributeVector * ret(_attr);
            _attr = NULL;
            return ret;
        }

        mutable AttributeVector * _attr;
    };

    class EnumModifier
    {
        std::unique_lock<std::shared_timed_mutex> _enumLock;
    public:
        EnumModifier(std::shared_timed_mutex &lock,
                     attribute::InterlockGuard &interlockGuard)
            : _enumLock(lock)
        {
            (void) interlockGuard;
        }
        EnumModifier(EnumModifier &&rhs)
            : _enumLock(std::move(rhs._enumLock))
        {
        }
        EnumModifier &operator=(EnumModifier &&rhs)
        {
            _enumLock = std::move(rhs._enumLock);
            return *this;
        }
        virtual ~EnumModifier()
        {
        }
    };

    EnumModifier getEnumModifier();
    ValueModifier getValueModifier() { return ValueModifier(*this); }

    void updateUncommittedDocIdLimit(DocId doc) {
        if (_uncommittedDocIdLimit <= doc)  {
            _uncommittedDocIdLimit = doc + 1;
        }
    }

    void updateCommittedDocIdLimit(void) {
        if (_uncommittedDocIdLimit != 0) {
            if (_uncommittedDocIdLimit > _committedDocIdLimit) {
                std::atomic_thread_fence(std::memory_order_release);
                _committedDocIdLimit = _uncommittedDocIdLimit;
            }
            _uncommittedDocIdLimit = 0;
        }
    }
    
public:
    void incGeneration();
    void removeAllOldGenerations();

    generation_t getFirstUsedGeneration() const {
        return _genHandler.getFirstUsedGeneration();
    }

    generation_t getCurrentGeneration() const {
        return _genHandler.getCurrentGeneration();
    }

    virtual IExtendAttribute * getExtendInterface();

protected:
    /**
     * Returns the number of readers holding a generation guard.
     * Should be called by the writer thread.
     */
    uint32_t getGenerationRefCount(generation_t gen) const {
        return _genHandler.getGenerationRefCount(gen);
    }

    const GenerationHandler & getGenerationHandler() const {
        return _genHandler;
    }

    GenerationHandler & getGenerationHandler() {
        return _genHandler;
    }

    GenerationHolder & getGenerationHolder() {
        return _genHolder;
    }

    template<typename T>
    bool clearDoc(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc);

    template<typename T>
    bool update(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc, const T & v) __attribute__((noinline));

    template<typename T>
    bool append(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc, const T &v, int32_t w, bool doCount = true) __attribute__((noinline));
    template<typename T, typename Accessor>
    bool append(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc, Accessor & ac) __attribute__((noinline));

    template<typename T>
    bool remove(ChangeVectorT< ChangeTemplate<T> > & changes, DocId doc, const T &v, int32_t w);

    template<typename T>
    bool adjustWeight(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc, const T &v, const ArithmeticValueUpdate &wd);

    template <typename T>
    static int32_t
    applyWeightChange(int32_t weight, const ChangeTemplate<T> &weightChange) {
        if (weightChange._type == ChangeBase::INCREASEWEIGHT) {
            return weight + weightChange._weight;
        } else if (weightChange._type == ChangeBase::MULWEIGHT) {
            return weight * weightChange._weight;
        } else if (weightChange._type == ChangeBase::DIVWEIGHT) {
            return weight / weightChange._weight;
        }
        return weight;
    }

    template<typename T>
    bool applyArithmetic(ChangeVectorT< ChangeTemplate<T> > &changes, DocId doc, const T &v, const ArithmeticValueUpdate & arithm);

    static double round(double v, double & r) { return r = v; }
    static largeint_t round(double v, largeint_t &r) { return r = static_cast<largeint_t>(::floor(v+0.5)); }

    template <typename BaseType, typename ChangeData>
    static BaseType
    applyArithmetic(const BaseType &value,
                    const ChangeTemplate<ChangeData> & arithmetic)
    {
        typedef typename ChangeData::DataType LargeType;
        if (attribute::isUndefined(value)) {
            return value;
        } else if (arithmetic._type == ChangeBase::ADD) {
            return value + static_cast<LargeType>(arithmetic._arithOperand);
        } else if (arithmetic._type == ChangeBase::SUB) {
            return value - static_cast<LargeType>(arithmetic._arithOperand);
        } else if (arithmetic._type == ChangeBase::MUL) {
            LargeType r;
            return round((static_cast<double>(value) *
                          arithmetic._arithOperand), r);
        } else if (arithmetic._type == ChangeBase::DIV) {
            LargeType r;
            return round(static_cast<double>(value) /
                         arithmetic._arithOperand, r);
        }
        return value;
    }

    virtual AddressSpace getEnumStoreAddressSpaceUsage() const;

    virtual AddressSpace getMultiValueAddressSpaceUsage() const;

public:
    DECLARE_IDENTIFIABLE_ABSTRACT(AttributeVector);
    bool isLoaded() const { return _loaded; }

    /** Return the fixed length of the attribute. If 0 then you must inquire each document. */
    virtual size_t getFixedWidth() const override { return _config.basicType().fixedSize(); }
    const Config &getConfig() const { return _config; }
    BasicType getInternalBasicType() const { return _config.basicType(); }
    CollectionType getInternalCollectionType() const { return _config.collectionType(); }
    const BaseName & getBaseFileName() const { return _baseFileName; }
    void setBaseFileName(const vespalib::stringref & name) { _baseFileName = name; }

    // Implements IAttributeVector
    virtual const vespalib::string & getName(void) const {
        return _baseFileName.getAttributeName();
    }

    virtual bool hasMultiValue() const {
        return _config.collectionType().isMultiValue();
    }

    virtual bool hasWeightedSetType() const {
        return _config.collectionType().isWeightedSet();
    }

    bool hasArrayType() const { return _config.collectionType().isArray(); }
    virtual bool hasEnum() const { return _hasEnum; }
    bool hasSortedEnum() const { return _hasSortedEnum; }
    virtual bool hasEnum2Value() const { return false; }
    virtual uint32_t getMaxValueCount() const { return _highestValueCount; }
    uint32_t getEnumMax() const { return _enumMax; }

    // Implements IAttributeVector
    virtual uint32_t getNumDocs(void) const { return _status.getNumDocs(); }
    uint32_t getCommittedDocIdLimit(void) const { return _committedDocIdLimit; }
    uint32_t & getCommittedDocIdLimitRef(void) { return _committedDocIdLimit; }
    void setCommittedDocIdLimit(uint32_t committedDocIdLimit) {
        _committedDocIdLimit = committedDocIdLimit;
    }

    const Status & getStatus() const { return _status; }
    Status & getStatus() { return _status; }

    AddressSpaceUsage getAddressSpaceUsage() const;

    // Implements IAttributeVector
    virtual BasicType::Type getBasicType() const {
        return getInternalBasicType().type();
    }

    virtual CollectionType::Type getCollectionType() const {
        return getInternalCollectionType().type();
    }

    /**
     * Updates the base file name of this attribute vector and saves
     * it to file(s)
     */
    bool saveAs(const vespalib::stringref &baseFileName);

    /**
     * Updates the base file name of this attribute vector and saves
     * it using the given saveTarget
     */
    bool saveAs(const vespalib::stringref &baseFileName,
                IAttributeSaveTarget &saveTarget);

    /** Saves this attribute vector to file(s) **/
    bool save();

    /** Saves this attribute vector using the given saveTarget **/
    bool save(IAttributeSaveTarget & saveTarget);

    IAttributeSaveTarget::Config createSaveTargetConfig() const;

    /** Returns whether this attribute has load data files on disk **/
    bool hasLoadData() const;

    bool isEnumeratedSaveFormat(void) const;
    bool load();
    void commit(bool forceStatUpdate = false);
    void commit(uint64_t firstSyncToken, uint64_t lastSyncToken);
    void setCreateSerialNum(uint64_t createSerialNum);
    uint64_t getCreateSerialNum(void) const;
    virtual uint32_t getVersion() const;

////// Interface to access single documents.
    /**
     * Interface to access the individual elements both for update and
     * retrival are type specific.  They are accessed by their proper
     * type.
     */
    /** Get number of values per document.  */
    virtual uint32_t getValueCount(DocId doc) const = 0;

    virtual uint32_t clearDoc(DocId doc) = 0;
    virtual largeint_t getDefaultValue() const = 0;
    virtual EnumHandle getEnum(DocId doc)  const = 0;
    virtual const char * getString(DocId doc, char * v, size_t sz) const = 0;
    virtual largeint_t getInt(DocId doc) const = 0;
    virtual double getFloat(DocId doc)   const = 0;
    virtual void getEnumValue(const EnumHandle *v, uint32_t *e, uint32_t sz) const = 0;

    uint32_t getEnumValue(EnumHandle eh) const {
        uint32_t e(0);
        getEnumValue(&eh, &e, 1);
        return e;
    }

    // Implements IAttributeVector
    virtual uint32_t get(DocId doc, EnumHandle *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, vespalib::string *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, const char **v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, largeint_t *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, double *v, uint32_t sz) const = 0;

    // Implements IAttributeVector
    virtual uint32_t get(DocId doc, WeightedEnum *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, WeightedString *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, WeightedConstChar *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, WeightedInt *v, uint32_t sz) const = 0;
    virtual uint32_t get(DocId doc, WeightedFloat *v, uint32_t sz) const = 0;
    virtual int32_t getWeight(DocId doc, uint32_t idx) const;

    // Implements IAttributeVector
    virtual bool findEnum(const char *value, EnumHandle &e) const {
        (void) value;
        (void) e;
        return false;
    }

///// Modify API
    virtual void onCommit() = 0;
    virtual bool addDoc(DocId &doc) = 0;
    virtual bool addDocs(DocId & startDoc, DocId & lastDoc, uint32_t numDocs);
    virtual bool addDocs(uint32_t numDocs);
    bool apply(DocId doc, const MapValueUpdate &map);

////// Search API

    // type-safe down-cast to attribute supporting direct document weight iterators
    virtual const IDocumentWeightAttribute *asDocumentWeightAttribute() const { return nullptr; }

    /**
       - Search for equality
       - Range search
    */

    class SearchContext : public vespalib::noncopyable
    {
        template <class SC> friend class AttributeIteratorT;
        template <class SC> friend class FilterAttributeIteratorT;
        template <class PL> friend class AttributePostingListIteratorT;
        template <class PL> friend class FilterAttributePostingListIteratorT;
    public:
        class Params {
            using IAttributeVector = attribute::IAttributeVector;
        public:
            Params();
            bool useBitVector() const { return _useBitVector; }
            const IAttributeVector * diversityAttribute() const { return _diversityAttribute; }
            size_t diversityCutoffGroups() const { return _diversityCutoffGroups; }
            bool diversityCutoffStrict() const { return _diversityCutoffStrict; }

            Params & useBitVector(bool value) {
                _useBitVector = value;
                return *this;
            }
            Params & diversityAttribute(const IAttributeVector * value) {
                _diversityAttribute = value;
                return *this;
            }
            Params & diversityCutoffGroups(size_t groups) {
                _diversityCutoffGroups = groups;
                return *this;
            }
            Params & diversityCutoffStrict(bool strict) {
                _diversityCutoffStrict = strict;
                return *this;
            }
        private:
            const IAttributeVector * _diversityAttribute;
            size_t                   _diversityCutoffGroups;
            bool                     _useBitVector;
            bool                     _diversityCutoffStrict;
        };
        typedef std::unique_ptr<SearchContext> UP;
        virtual ~SearchContext();
        virtual unsigned int approximateHits() const;
        static QueryTermSimple::UP decodeQuery(const QueryPacketT & searchSpec);

        /**
         * Creates an attribute search iterator associated with this
         * search context.
         *
         * @return attribute search iterator
         *
         * @param matchData the attribute match data used when
         * unpacking data for a hit
         *
         * @param strict whether the iterator should be strict or not
         *
         * @param useBitVector whether bitvectors should be used when available
         **/
        virtual queryeval::SearchIterator::UP
        createIterator(fef::TermFieldMatchData *matchData, bool strict);

        /**
         * Creates an attribute search iterator associated with this
         * search context.  Postings lists are not used.
         *
         * @return attribute search iterator
         *
         * @param matchData the attribute match data used when
         * unpacking data for a hit
         *
         * @param strict whether the iterator should be strict or not
         **/
        virtual queryeval::SearchIterator::UP
        createFilterIterator(fef::TermFieldMatchData *matchData, bool strict);

        /*
         * Create temporary posting lists.  Should be called before
         * createIterator is called.
         */
        virtual void fetchPostings(bool strict);
        bool cmp(DocId docId, int32_t &weight) const { return onCmp(docId, weight); }
        bool cmp(DocId docId) const { return onCmp(docId); }
        const AttributeVector & attribute() const { return _attr; }
        virtual bool valid() const { return false; }
        virtual Int64Range getAsIntegerTerm() const { return Int64Range(); }

        virtual const QueryTermBase & queryTerm() const {
            return *static_cast<const QueryTermBase *>(NULL);
        }

    protected:
        SearchContext(const AttributeVector &attr);
    private:
        virtual bool onCmp(DocId docId, int32_t &weight) const = 0;
        virtual bool onCmp(DocId docId) const = 0;

        const AttributeVector & _attr;
    protected:
        attribute::IPostingListSearchContext *_plsc;

        bool getIsFilter(void) const { return _attr.getConfig().getIsFilter(); }
    };

    SearchContext::UP getSearch(const QueryPacketT &searchSpec, const SearchContext::Params & params) const;
    virtual SearchContext::UP getSearch(QueryTermSimple::UP term, const SearchContext::Params & params) const = 0;
    virtual const EnumStoreBase *getEnumStoreBase() const { return nullptr; }
    virtual const attribute::MultiValueMapping2Base *getMultiValueBase() const { return nullptr; }
private:
    void divideByZeroWarning();
    virtual bool applyWeight(DocId doc, const FieldValue &fv, const ArithmeticValueUpdate &wAdjust);
    virtual void onSave(IAttributeSaveTarget & saveTarget);
    virtual bool onLoad();
    bool headerTypeOK(const vespalib::GenericHeader &header) const;
    std::unique_ptr<Fast_BufferedFile> openFile(const char *suffix);
    FileUtil::LoadedBuffer::UP loadFile(const char *suffix);


    BaseName               _baseFileName;
    Config                 _config;
    std::shared_ptr<attribute::Interlock> _interlock;
    std::shared_timed_mutex _enumLock;
    GenerationHandler      _genHandler;
    GenerationHolder       _genHolder;
    Status                 _status;
    int                    _highestValueCount;
    uint32_t               _enumMax;
    uint32_t               _committedDocIdLimit; // docid limit for search
    uint32_t               _uncommittedDocIdLimit; // based on queued changes
    uint64_t               _createSerialNum;
    uint64_t               _compactLidSpaceGeneration; 
    bool                   _hasEnum;
    bool                   _hasSortedEnum;
    bool                   _loaded;
    bool                   _enableEnumeratedSave;
    fastos::TimeStamp      _nextStatUpdateTime;

////// Locking strategy interface. only available from the Guards.
    /**
     * Used to guard that a value you reference will always reference
     * a value. It might not be the same value, but at least it will
     * be a value for that document.  The guarantee holds as long as
     * the guard is alive.
    */
    GenerationHandler::Guard takeGenerationGuard() { return _genHandler.takeGuard(); }

    /// Clean up [0, firstUsed>
    virtual void removeOldGenerations(generation_t firstUsed) { (void) firstUsed; }
    virtual void onGenerationChange(generation_t generation) { (void) generation; }
    virtual void onUpdateStat() = 0;
    /**
     * Used to regulate access to critical resources. Apply the
     * reader/writer guards.
     */
    std::shared_timed_mutex & getEnumLock() { return _enumLock; }

    friend class ComponentGuard<AttributeVector>;
    friend class AttributeEnumGuard;
    friend class AttributeValueGuard;
    friend class AttributeTest;
    friend class AttributeManagerTest;
public:
    /**
     * Should be called by the writer thread.
     */
    void updateFirstUsedGeneration(void) {
        _genHandler.updateFirstUsedGeneration();
    }

    /**
     * Returns true if we might still have readers.  False positives
     * are possible if writer hasn't updated first used generation
     * after last reader left.
     */
    bool hasReaders(void) const { return _genHandler.hasReaders(); }

    /**
     * Add reserved initial document with docId 0 and undefined value.
     */
    void addReservedDoc(void);

    void enableEnumeratedSave(bool enable = true);

    /*
     * Temporary method, used by unit tests to enable enumerated load
     * until it can be enabled by default.
     */
    static void enableEnumeratedLoad(void);

    bool getEnumeratedSave(void) const { return _hasEnum && _enableEnumeratedSave; }

    virtual attribute::IPostingListAttributeBase * getIPostingListAttributeBase();
    bool hasPostings(void);
    virtual uint64_t getUniqueValueCount(void) const;
    virtual uint64_t getTotalValueCount(void) const;
    virtual void compactLidSpace(uint32_t wantedLidLimit);
    virtual void clearDocs(DocId lidLow, DocId lidLimit);
    bool wantShrinkLidSpace(void) const { return _committedDocIdLimit < getNumDocs(); }
    virtual bool canShrinkLidSpace(void) const;
    void shrinkLidSpace(void);
    virtual void onShrinkLidSpace(void);

    void setInterlock(const std::shared_ptr<attribute::Interlock> &interlock);

    const std::shared_ptr<attribute::Interlock> &getInterlock() const
    {
        return _interlock;
    }

    std::unique_ptr<AttributeSaver> initSave();

    virtual std::unique_ptr<AttributeSaver> onInitSave();

    virtual uint64_t getEstimatedSaveByteSize() const;
};

}

