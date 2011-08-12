/**************************************************************************************

Copyright © 2004-2010 VMware, Inc. All rights reserved.

Written by Mark Venguerov 2004 - 2010

**************************************************************************************/

#ifndef _MVSTOREIMPL_H_
#define _MVSTOREIMPL_H_

#include "mvstore.h"
#include "session.h"

using namespace MVStore;

namespace MVStoreKernel
{

extern	MemAlloc *createMemAlloc(size_t,bool fMulti);
extern	RC		copyV(const Value& from,Value& to,MemAlloc *ma);
extern	RC		copyV(const Value *from,ulong nv,Value *&to,MemAlloc *ma);
extern	bool	operator==(const Value& lhs, const Value& rhs);
inline	bool	operator!=(const Value& lhs, const Value& rhs) {return !(lhs==rhs);}
extern	size_t	serSize(const Value& val,bool full=false);
extern	size_t	serSize(const PID &id);
extern	byte	*serialize(const Value& val,byte *buf,bool full=false);
extern	byte	*serialize(const PID &id,byte *buf);
extern	RC		deserialize(Value& val,const byte *&buf,const byte *const ebuf,MemAlloc*,bool,bool full=false);
extern	RC		deserialize(PID& id,const byte *&buf,const byte *const ebuf);
extern	RC		streamToValue(IStream *str,Value& val,MemAlloc*);
extern	RC		convV(const Value& src,Value& dst,ValueType type);
extern	RC		derefValue(const Value& src,Value& dst,Session *ses);
extern	RC		convURL(const Value& src,Value& dst,HEAP_TYPE alloc);
extern	bool	compatible(QualifiedValue&,QualifiedValue&);
extern	bool	compatibleMulDiv(Value&,uint16_t units,bool fDiv);
extern	Units	getUnits(const char *suffix,size_t l);
extern	void	freeV(Value *v,ulong nv,MemAlloc*);
extern	void	freeV0(Value& v);
__forceinline	void freeV(Value& v) {if ((v.flags&HEAP_TYPE_MASK)!=NO_HEAP) freeV0(v);}

#define PIDKeySize		(sizeof(uint64_t)+sizeof(IdentityID))

class IntNav : public INav
{
public:
	virtual	INav		*clone(MemAlloc *ma) const = 0;
	virtual	const Value	*navigateNR(GO_DIR op,ElementID=STORE_COLLECTION_ID) = 0;
	virtual	void		release() = 0;
};

struct PropertyList
{
	PropertyID	*props;
	unsigned	nProps;
};

struct TDescriptor
{
	Value		*vals;
	unsigned	nValues;
};

#define	MODP_EIDS	0x0001
#define	MODP_NEID	0x0002
#define MODP_NCPY	0x0004

#define	ORDER_EXPR	0x8000

struct OrderSegQ
{
	union {
		PropertyID	pid;
		class Expr	*expr;
	};
	uint16_t		flags;
	uint16_t		lPref;
};

struct ParentInfo
{
	ParentInfo	*next;
	class	PIN	*pin;
	PID			id;
	PropertyID	propID;
	ElementID	eid;
};

/**
 * PIN flags (continuation of flags in mvstore.h)
 */

#define	PIN_NO_FREE					0x80000000	/**< don't free properties in destructor */
#define	PIN_READONLY				0x02000000	/**< readonly pin - from remote pin cache */
#define	PIN_DELETED					0x01000000	/**< soft-deleted pin (only with MODE_DELETED or IDumpStore) */
#define	PIN_CLASS					0x00800000	/**< pin represents a class or a relation (set in commitPINs) */
#define	PIN_TRANSFORMED				0x00400000	/**< pin is a result of some transformation (other than projection) */
#define	PIN_PROJECTED				0x00200000	/**< pin is a result of a projection of a stored pin */

class PIN : public IPIN
{
	const				PID	id;
	Session				*const ses;
	Value				*properties;
	uint32_t			nProperties;
	uint32_t			mode;
	mutable	PageAddr	addr;
	PIN					*nextPIN;
	ParentInfo			*parent;
	union {
		uint32_t		stamp;
		size_t			length;
	};

public:
	PIN(Session *s,const PID& i,const PageAddr& a,ulong md=0,Value *vals=NULL,ulong nvals=0)
		: id(i),ses(s),properties(vals),nProperties(nvals),mode(md),addr(a),nextPIN(NULL) {parent=NULL; length=0;}
	virtual		~PIN();
	const PID&	getPID() const;
	bool		isLocal() const;
	bool		isCommitted() const;
	bool		isReadonly() const;
	bool		canNotify() const;
	bool		isReplicated() const;
	bool		canBeReplicated() const;
	bool		isHidden() const;
	bool		isDeleted() const;
	bool		isClass() const;
	bool		isTransformed() const;
	bool		isProjected() const;
	uint32_t	getNumberOfProperties() const;
	const Value	*getValueByIndex(unsigned idx) const;
	const Value	*getValue(PropertyID pid) const;
	char		*getURI() const;
	uint32_t	getStamp() const;
	RC			getPINValue(Value& res) const;
	RC			getPINValue(Value& res,Session *ses) const;
	bool		testClassMembership(ClassID,const Value *params=NULL,unsigned nParams=0) const;
	bool		defined(const PropertyID *pids,unsigned nProps) const;
	IPIN		*clone(const Value *overwriteValues=NULL,unsigned nOverwriteValues=0,unsigned mode=0);
	IPIN		*project(const PropertyID *properties,unsigned nProperties,const PropertyID *newProps=NULL,unsigned mode=0);
	RC			modify(const Value *values,unsigned nValues,unsigned mode,const ElementID *eids,unsigned*);
	RC			makePart(IPIN *parent,PropertyID pid,ElementID=STORE_COLLECTION_ID);
	RC			makePart(const PID& parentID,PropertyID pid,ElementID=STORE_COLLECTION_ID);
	RC			setExpiration(uint32_t);
	RC			setNotification(bool fReset=false);
	RC			setReplicated();
	RC			setNoIndex();
	RC			deletePIN();
	RC			undelete();
	RC			refresh(bool);
	void		destroy();

	void		operator delete(void *p) {if (((PIN*)p)->ses!=NULL) ((PIN*)p)->ses->free(p);/* else ???*/}
	RC			modify(const Value *pv,ulong epos,ulong eid,ulong flags,Session *ses);
	const PageAddr& getAddr() const {return addr;}
	__forceinline const Value *findProperty(PropertyID pid) const {return mv_bsrcmp<Value,PropertyID>(pid,properties,nProperties);}
	ElementID	getPrefix(StoreCtx *ctx) const {return id.pid==STORE_INVALID_PID||id.ident==STORE_INVALID_IDENTITY?ctx->getPrefix():StoreCtx::genPrefix(ushort(id.pid>>48));}
	static PIN*	getPIN(const PID& id,VersionID vid,Session *ses,ulong mode=0);
	static const Value *findElement(const Value *pv,ulong eid) {
		assert(pv!=NULL && pv->type==VT_ARRAY);
		if (pv->length!=0) {
			if (eid==STORE_FIRST_ELEMENT) return &pv->varray[0];
			if (eid==STORE_LAST_ELEMENT) return &pv->varray[pv->length-1];
			for (ulong i=0; i<pv->length; i++) if (pv->varray[i].eid==eid) return &pv->varray[i];
		}
		return NULL;
	}
	static	Value*	normalize(const Value *pv,uint32_t& nv,ulong f,ElementID prefix,MemAlloc *ma);
	static	int		__cdecl cmpLength(const void *v1, const void *v2);
	static	ulong	getCardinality(const PID& ref,ulong propID);
	static	const	PID defPID;
	friend	class	EncodePB;
	friend	class	ProtoBufStreamIn;
	friend	class	SyncStreamIn;
	friend	class	ServerStreamIn;
	friend	class	FullScan;
	friend	class	QueryPrc;
	friend	class	Cursor;
	friend	class	Stmt;
	friend	class	SessionX;
	friend	class	Classifier;
	friend	class	ClassPropIndex;
	friend	class	NetMgr;
	friend	class	RPIN;
	friend	class	PINEx;
};

inline bool isRemote(const PID& id) {return id.ident!=STORE_OWNER&&id.ident!=STORE_INVALID_IDENTITY||id.pid!=STORE_INVALID_PID&&ushort(id.pid>>48)!=StoreCtx::get()->storeID;}

class SessionX : public ISession
{
	Session			*const ses;
private:
	void operator	delete(void *p) {}
	bool			login(const char *ident,const char *pwd);
	bool			checkAdmin() {return ses!=NULL&&ses->identity==STORE_OWNER;}
public:
	SessionX(Session *s) : ses(s) {}
	friend		ISession	*ISession::startSession(MVStoreCtx,const char*,const char*);
	static		SessionX	*create(Session *ses) {return new(ses) SessionX(ses);}
	ISession	*clone(const char* =NULL) const;
	RC			attachToCurrentThread();
	RC			detachFromCurrentThread();

	void		setInterfaceMode(unsigned);
	unsigned	getInterfaceMode() const;
	RC			setURIBase(const char *ns);
	RC			addURIPrefix(const char *name,const char *URIprefix);
	void		setDefaultExpiration(uint64_t defExp);
	void		changeTraceMode(unsigned mask,bool fReset=false);
	void		setTrace(ITrace *);
	void		terminate();

	RC			mapURIs(unsigned nURIs,URIMap URIs[]);
	RC			getURI(uint32_t,char buf[],size_t& lbuf);

	IdentityID	getIdentityID(const char *identity);
	RC			impersonate(const char *identity);
	IdentityID	storeIdentity(const char *ident,const char *pwd,bool fMayInsert=true,const unsigned char *cert=NULL,unsigned lcert=0);
	IdentityID	loadIdentity(const char *identity,const unsigned char *pwd,unsigned lPwd,bool fMayInsert=true,const unsigned char *cert=NULL,unsigned lcert=0);
	RC			setInsertPermission(IdentityID,bool fMayInsert=true);
	size_t		getStoreIdentityName(char buf[],size_t lbuf);
	size_t		getIdentityName(IdentityID,char buf[],size_t lbuf);
	size_t		getCertificate(IdentityID,unsigned char buf[],size_t lbuf);
	RC			changePassword(IdentityID,const char *oldPwd,const char *newPwd);
	RC			changeCertificate(IdentityID,const char *pwd,const unsigned char *cert,unsigned lcert);
	RC			changeStoreIdentity(const char *newIdentity);
	IdentityID	getCurrentIdentityID() const;

	unsigned	getStoreID(const PID&);
	unsigned	getLocalStoreID();

	IStmt		*createStmt(STMT_OP=STMT_QUERY,unsigned mode=0);
	IStmt		*createStmt(const char *queryStr,const URIID *ids=NULL,unsigned nids=0,CompilationError *ce=NULL);
	IExprTree	*expr(ExprOp op,unsigned nOperands,const Value *operands,unsigned flags=0);
	IExprTree	*createExprTree(const char *str,const URIID *ids=NULL,unsigned nids=0,CompilationError *ce=NULL);
	IExpr		*createExpr(const char *str,const URIID *ids=NULL,unsigned nids=0,CompilationError *ce=NULL);
	RC			getTypeName(ValueType type,char buf[],size_t lbuf);
	void		abortQuery();

	RC			execute(const char *str,size_t lstr,char **result=NULL,const URIID *ids=NULL,unsigned nids=0,
						const Value *params=NULL,unsigned nParams=0,CompilationError *ce=NULL,uint64_t *nProcessed=NULL,
						unsigned nProcess=~0u,unsigned nSkip=0);

	RC			createInputStream(IStreamIn *&in,IStreamIn *out=NULL,size_t lbuf=0);

	RC			getClassID(const char *className,ClassID& cid);
	RC			enableClassNotifications(ClassID,unsigned notifications);
	RC			rebuildIndices(const ClassID *cidx=NULL,unsigned nClasses=0);
	RC			rebuildIndexFT();
	RC			createIndexNav(ClassID,IndexNav *&nav);
	RC			listValues(ClassID cid,PropertyID pid,ValueType vt,IndexNav *&ven);
	RC			listWords(const char *query,StringEnum *&sen);
	RC			getClassInfo(ClassID,IPIN*&);
	
	IPIN		*getPIN(const PID& id,unsigned=0);
	IPIN		*getPIN(const Value& id,unsigned=0);
	IPIN		*getPINByURI(const char *uri,unsigned mode=0);
	RC			getValues(Value *vals,unsigned nVals,const PID& id);
	RC			getValue(Value& res,const PID& id,PropertyID,ElementID=STORE_COLLECTION_ID);
	RC			getValue(Value& res,const PID& id);
	bool		isCached(const PID& id);
	IPIN		*createUncommittedPIN(Value *values=NULL,unsigned nValues=0,unsigned mode=0,const PID *original=NULL);
	RC			createPIN(PID& res,const Value values[],unsigned nValues,unsigned mode=0,const AllocCtrl* =NULL);
	RC			commitPINs(IPIN * const *newPins,unsigned nNew,unsigned mode=0,const AllocCtrl* =NULL,const Value *params=NULL,unsigned nParams=0);
	RC			modifyPIN(const PID& id,const Value *values,unsigned nValues,unsigned mode=0,const ElementID *eids=NULL,unsigned *pNFailed=NULL,const Value *params=NULL,unsigned nParams=0);
	RC			deletePINs(IPIN **pins,unsigned nPins,unsigned mode=0);
	RC			deletePINs(const PID *pids,unsigned nPids,unsigned mode=0);
	RC			undeletePINs(const PID *pids,unsigned nPids);
	RC			setPINAllocationParameters(const AllocCtrl *ac);

	RC			setIsolationLevel(TXI_LEVEL);
	RC			startTransaction(TX_TYPE=TXT_READWRITE,TXI_LEVEL=TXI_DEFAULT);
	RC			commit(bool fAll);
	RC			rollback(bool fAll);

//	RC			dumpStore(IDumpStore*&,bool);
//	RC			storeInspector(IStoreInspector*& si,bool);
//	void		mapStoreID(unsigned short oldStoreID);
	RC			reservePage(uint32_t);

	RC			copyValue(const Value& src,Value& dest);
	RC			convertValue(const Value& oldValue,Value& newValue,ValueType newType);
	RC			parseValue(const char *p,size_t l,Value& res,CompilationError *ce=NULL);
	RC			parseValues(const char *p,size_t l,Value *&res,unsigned& nValues,CompilationError *ce=NULL,char delimiter=',');
	int			compareValues(const Value& v1,const Value& v2,bool fNCase=false);
	void		freeValues(Value *vals,unsigned nVals);
	void		freeValue(Value& val);

	void		setTimeZone(int64_t tz);
	RC			convDateTime(uint64_t dt,DateTime& dts,bool fUTC=true) const;
	RC			convDateTime(const DateTime& dts,uint64_t& dt,bool fUTC=true) const;

	RC			setStopWordTable(const char **words,uint32_t nWords,PropertyID pid=STORE_INVALID_PROPID,
													bool fOverwriteDefault=false,bool fSessionOnly=false);

	void		*alloc(size_t);
	void		*realloc(void *,size_t);
	void		free(void *);
};

};

#endif
