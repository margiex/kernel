/**************************************************************************************

Copyright © 2004-2010 VMware, Inc. All rights reserved.

Written by Mark Venguerov 2004 - 2010

**************************************************************************************/

#ifndef _QUERY_H_
#define _QUERY_H_

#include "mvstoreimpl.h"
#include "session.h"
#include "idxtree.h"
#include "txmgr.h"
#include "pinex.h"

using namespace MVStore;

namespace MVStoreKernel
{

#define	QRY_PARAMS		0x80000000
#define	QRY_ORDEXPR		0x40000000
#define	QRY_IDXEXPR		0x20000000
#define	QRY_CPARAMS		0x10000000

class SOutCtx;
class SInCtx;

struct IndexExpr
{
	class	Expr		*expr;
	ulong				nProps;
	PropertyID			props[1];
	void				*operator new(size_t s,unsigned np,MemAlloc *ma) throw() {return ma->malloc(s+(np==0?0:np-1)*sizeof(PropertyID));}
};

struct CondIdx
{
	CondIdx				*next;
	const	IndexSeg	ks;
	const	ushort		param;
	MemAlloc *const		ma;
	IndexExpr			*expr;
	CondIdx(const IndexSeg& dscr,ushort parm,MemAlloc *m,IndexExpr *exp=NULL) : next(NULL),ks(dscr),param(parm),ma(m),expr(exp) {}
	~CondIdx() {if (expr!=NULL) {ma->free(expr->expr); ma->free(expr);}}
	CondIdx	*clone(MemAlloc *ma) const;
};

struct CondEJ
{
	CondEJ				*next;
	const	PropertyID	propID1;
	const	PropertyID	propID2;
	const	ushort		flags;
	CondEJ(PropertyID pid1,PropertyID pid2,ushort f) : propID1(pid1),propID2(pid2),flags(f) {}
};

struct CondFT
{
	CondFT			*next;
	const	char	*str;
	unsigned		flags;
	unsigned		nPids;
	PropertyID		pids[1];
	CondFT(CondFT *nxt,const char *s,unsigned f,const PropertyID *ps,unsigned nps) 
		: next(nxt),str(s),flags(f),nPids(nps) {if (ps!=NULL && nps>0) memcpy(pids,ps,nps*sizeof(PropertyID));}
	void	*operator new(size_t s,unsigned nps,MemAlloc *ma) throw() {return ma->malloc(s+(nps==0?0:nps-1)*sizeof(PropertyID));}
};

enum SelectType
{
	SEL_COUNT, SEL_VALUE, SEL_VALUESET, SEL_DERIVED, SEL_DERIVEDSET, SEL_PROJECTED, SEL_PINSET, SEL_COMPOUND, SEL_COMP_DERIVED
};

enum RenderPart
{
	RP_SELECT, RP_FROM, RP_WHERE, RP_MATCH
};

#define	QRY_SIMPLE	QRY_ALL_SETOP

class QVar
{
protected:
	QVar			*next;
	const	QVarID	id;
	const	byte	type;
	SelectType		stype;
	DistinctType	dtype;
	MemAlloc *const	ma;
	char			*name;
	TDescriptor		*dscr;
	unsigned		nDscr;
	PropertyList	*varProps;
	unsigned		nVarProps;
	union {
		Expr		*cond;
		Expr		**conds;
	};
	unsigned		nConds;
	union {
		Expr		*havingCond;
		Expr		**havingConds;
	};
	unsigned		nHavingConds;
	OrderSegQ		*groupBy;
	unsigned		nGroupBy;
	PropDNF			*condProps;
	size_t			lProps;
	bool			fHasParent;
	QVar(QVarID i,byte ty,MemAlloc *m);
public:
	virtual			~QVar();
	virtual	RC		clone(MemAlloc *m,QVar*&,bool fClass) const = 0;
	virtual	RC		build(class QueryCtx& qctx,class QueryOp *&qop) const = 0;
	virtual	size_t	serSize() const;
	virtual	byte	*serialize(byte *buf) const = 0;
	virtual	RC		render(RenderPart,SOutCtx&) const;
	virtual	RC		render(SOutCtx&) const;
	virtual	const	QVar *getRefVar(unsigned refN) const;
	static	RC		deserialize(const byte *&buf,const byte *const ebuf,MemAlloc *ma,QVar*& res);
	void	operator delete(void *p) {if (p!=NULL) ((QVar*)p)->ma->free(p);}
	QVarID			getID() const {return id;}
	byte			getType() const {return type;}
	bool			isMulti() const {return type<QRY_ALL_SETOP;}
	RC				clone(QVar *cloned,bool fClass=false) const;
	byte			*serQV(byte *buf) const;
	friend	class	Stmt;
	friend	class	SimpleVar;
	friend	class	Classifier;
	friend	class	ClassPropIndex;
	friend	class	QueryPrc;
	friend	class	QueryCtx;
	friend	class	SInCtx;
	friend	class	SOutCtx;
};

class SimpleVar : public QVar
{
	ClassSpec		*classes;
	ulong			nClasses;
	CondIdx			*condIdx;
	CondIdx			*lastCondIdx;
	unsigned		nCondIdx;
	PID				*pids;
	unsigned		nPids;
	Stmt			*subq;
	CondFT			*condFT;
	PropertyID		*props;
	unsigned		nProps;
	bool			fOrProps;
	PathSeg			*path;
	unsigned		nPathSeg;
	SimpleVar(QVarID i,MemAlloc *m) : QVar(i,QRY_SIMPLE,m),classes(NULL),nClasses(0),condIdx(NULL),lastCondIdx(NULL),nCondIdx(0),
									pids(NULL),nPids(0),subq(NULL),condFT(NULL),props(NULL),nProps(0),fOrProps(false),path(NULL),nPathSeg(0) {}
	virtual			~SimpleVar();
	RC				clone(MemAlloc *m,QVar*&,bool fClass) const;
	RC				build(class QueryCtx& qctx,class QueryOp *&qop) const;
	size_t			serSize() const;
	byte			*serialize(byte *buf) const;
	static	RC		deserialize(const byte *&buf,const byte *const ebuf,QVarID,MemAlloc *ma,QVar*& res);
	RC				render(RenderPart,SOutCtx&) const;
public:
	friend	class	Stmt;
	friend	class	Class;
	friend	class	Classifier;
	friend	class	ClassPropIndex;
	friend	class	SInCtx;
	friend	class	SOutCtx;
	friend	RC		QVar::render(SOutCtx&) const;
	friend	RC		QVar::deserialize(const byte *&buf,const byte *const ebuf,MemAlloc *ma,QVar*& res);
};

union QVarRef 
{
	QVar	*var;
	QVarID	varID;
};

class SetOpVar : public QVar
{
	friend	class	Stmt;
	friend	class	SInCtx;
	friend	RC		QVar::render(SOutCtx&) const;
	friend	RC		QVar::deserialize(const byte *&buf,const byte *const ebuf,MemAlloc *ma,QVar*& res);
	const unsigned	nVars;
	QVarRef			vars[2];
	void			*operator new(size_t s,unsigned nv,MemAlloc *m) {return m->malloc(s+int(nv-2)*sizeof(QVarRef));}
	SetOpVar(unsigned nv,QVarID i,byte ty,MemAlloc *m) : QVar(i,ty,m),nVars(nv) {}
	RC				clone(MemAlloc *m,QVar*&,bool fClass) const;
	RC				build(class QueryCtx& qctx,class QueryOp *&qop) const;
	size_t			serSize() const;
	byte			*serialize(byte *buf) const;
	static	RC		deserialize(const byte *&buf,const byte *const ebuf,QVarID,byte,MemAlloc *ma,QVar*& res);
	RC				render(RenderPart,SOutCtx&) const;
	RC				render(SOutCtx&) const;
};

class JoinVar : public QVar
{
	friend	class	Stmt;
	friend	class	SInCtx;
	friend	RC		QVar::deserialize(const byte *&buf,const byte *const ebuf,MemAlloc *ma,QVar*& res);
	const unsigned	nVars;
	CondEJ			*condEJ;
	QVarRef			vars[2];
	void			*operator new(size_t s,unsigned nv,MemAlloc *m) {return m->malloc(s+int(nv-2)*sizeof(QVarRef));}
	JoinVar(unsigned nv,QVarID i,byte ty,MemAlloc *m) : QVar(i,ty,m),nVars(nv),condEJ(NULL) {}
	virtual			~JoinVar();
	RC				clone(MemAlloc *m,QVar*&,bool fClass) const;
	RC				build(class QueryCtx& qctx,class QueryOp *&qop) const;
	size_t			serSize() const;
	byte			*serialize(byte *buf) const;
	static	RC		deserialize(const byte *&buf,const byte *const ebuf,QVarID,byte,MemAlloc *ma,QVar*& res);
	RC				render(RenderPart,SOutCtx&) const;
	const	QVar	*getRefVar(unsigned refN) const;
};

class Stmt : public IStmt
{
	const	STMT_OP	op;
	ulong			mode;
	MemAlloc		*const	ma;
	QVar			*top;
	unsigned		nTop;
	QVar			*vars;
	unsigned		nVars;
	OrderSegQ		*orderBy;
	unsigned		nOrderBy;
	Value			*values;
	unsigned		nValues;
public:
	Stmt(ulong md,MemAlloc *m,STMT_OP sop=STMT_QUERY) : op(sop),mode(md),ma(m),top(NULL),nTop(0),vars(NULL),nVars(0),orderBy(NULL),nOrderBy(0),values(NULL),nValues(0) {}
	virtual	~Stmt();
	QVarID	addVariable(const ClassSpec *classes=NULL,unsigned nClasses=0,IExprTree *cond=NULL);
	QVarID	addVariable(const PID& pid,PropertyID propID,IExprTree *cond=NULL);
	QVarID	addVariable(IStmt *qry);
	QVarID	setOp(QVarID leftVar,QVarID rightVar,QUERY_SETOP);
	QVarID	setOp(const QVarID *vars,unsigned nVars,QUERY_SETOP);
	QVarID	join(QVarID leftVar,QVarID rightVar,IExprTree *cond=NULL,QUERY_SETOP=QRY_JOIN,PropertyID=STORE_INVALID_PROPID);
	QVarID	join(const QVarID *vars,unsigned nVars,IExprTree *cond=NULL,QUERY_SETOP=QRY_JOIN,PropertyID=STORE_INVALID_PROPID);
	RC		setName(QVarID var,const char *name);
	RC		setDistinct(QVarID var,DistinctType dt);
	RC		addOutput(QVarID var,const Value *dscr,unsigned nDscr);
	RC		addCondition(QVarID var,IExprTree *cond,bool fHaving=false);
	RC		addConditionFT(QVarID var,const char *str,unsigned flags=0,const PropertyID *pids=NULL,unsigned nPids=0);
	RC		setPIDs(QVarID var,const PID *pids,unsigned nPids);
	RC		setPath(QVarID var,const PathSeg *segs,unsigned nSegs);
	RC		setPropCondition(QVarID var,const PropertyID *props,unsigned nProps,bool fOr=false);
	RC		setJoinProperties(QVarID var,const PropertyID *props,unsigned nProps);
	RC		setGroup(QVarID,const OrderSeg *order,unsigned nSegs);
	RC		setOrder(const OrderSeg *order,unsigned nSegs);
	RC		setValues(const Value *values,unsigned nValues);
	RC		setValuesNoCopy(const Value *values,unsigned nValues);
	STMT_OP	getOp() const;
	RC		execute(ICursor **result=NULL,const Value *params=NULL,unsigned nParams=0,unsigned nReturn=~0u,unsigned nSkip=0,unsigned long mode=0,uint64_t *nProcessed=NULL,TXI_LEVEL=TXI_DEFAULT) const;
	RC		asyncexec(IStmtCallback *cb,const Value *params=NULL,unsigned nParams=0,unsigned nProcess=~0u,unsigned nSkip=0,unsigned long mode=0,TXI_LEVEL=TXI_DEFAULT) const;
	RC		execute(IStreamOut*& result,const Value *params=NULL,unsigned nParams=0,unsigned nReturn=~0u,unsigned nSkip=0,unsigned long mode=0,TXI_LEVEL=TXI_DEFAULT) const;
	RC		count(uint64_t& cnt,const Value *params=NULL,unsigned nParams=0,unsigned long nAbort=~0u,unsigned long mode=0,TXI_LEVEL=TXI_DEFAULT) const;
	RC		exist(const Value *params=NULL,unsigned nParams=0,unsigned long mode=0,TXI_LEVEL=TXI_DEFAULT) const;
	RC		analyze(char *&plan,const Value *pars=NULL,unsigned nPars=0,unsigned long md=0) const;

	bool	isSatisfied(const IPIN *,const Value *pars=NULL,unsigned nPars=0,unsigned long mode=0) const;
	bool	isSatisfied(const IPIN *const *pins,unsigned nPins,const Value *pars=NULL,unsigned nPars=0,unsigned long mode=0) const;

	char	*toString(unsigned mode=0,const QName *qNames=NULL,unsigned nQNames=0) const;
	IStmt	*clone(STMT_OP=STMT_OP_ALL) const;
	Stmt	*clone(STMT_OP sop,MemAlloc *ma,bool fClass=false) const;
	void	trace(Session *ses,const char *op,RC rc,ulong cnt,const Value *pars,unsigned nPars,const Value *mods=NULL,unsigned nMods=0) const;
	void	destroy();

	bool	hasParams() const {return (mode&QRY_PARAMS)!=0;}
	bool	isClassOK() const {return top!=NULL && classOK(top);}
	bool	isPropOnly() const {return top!=NULL && top->type==QRY_SIMPLE && top->nConds==0 && ((SimpleVar*)top)->condFT==NULL 
								&& ((SimpleVar*)top)->nClasses==0 && ((SimpleVar*)top)->nCondIdx==0 && ((SimpleVar*)top)->nPids==0
								&& ((SimpleVar*)top)->props!=NULL && ((SimpleVar*)top)->nProps!=0;}
	bool	checkConditions(const PINEx *pin,const Value *pars,ulong nPars,MemAlloc *ma,ulong start=0,bool fIgnore=false) const;
	RC		normalize();
	RC		render(SOutCtx&) const;
	RC		setPath(QVarID var,const PathSeg *segs,unsigned nSegs,bool fCopy);
	QVar	*getTop() const {return top;}

	size_t		serSize() const;
	byte		*serialize(byte *buf) const;
	static	RC	deserialize(Stmt*&,const byte *&,const byte *const ebuf,MemAlloc*);
private:
	RC				connectVars();
	RC				processCondition(class ExprTree*,QVar *qv,int level=0);
	QVar			*findVar(QVarID id) const {QVar *qv=vars; while (qv!=NULL&&qv->id!=id) qv=qv->next; return qv;}
	RC				render(const QVar *qv,SOutCtx& out) const;
	static	bool	classOK(const QVar *);
	friend class	Class;
	friend class	Classifier;
	friend class	ClassPropIndex;
	friend class	QueryPrc;
	friend class	QueryCtx;
	friend class	SimpleVar;
	friend class	SInCtx;
};

class Cursor : public ICursor
{
	friend	class		Stmt;
	QueryOp				*queryOp;
	Session	*const		ses;
	const	uint64_t	nReturn;
	const	Value		*values;
	const	unsigned	nValues;
	const	ulong		mode;
	const	SelectType	stype;
	const	STMT_OP		op;
	TXID				txid;
	TXCID				txcid;
	uint64_t			cnt;
	TxSP				tx;
	bool				fSnapshot;
	void	operator	delete(void *p) {if (p!=NULL) ((Cursor*)p)->ses->free(p);}
	RC					skip();
public:
	Cursor(QueryOp *qop,uint64_t nRet,ulong md,const Value *vals,unsigned nV,Session *s,STMT_OP sop=STMT_QUERY,SelectType ste=SEL_PINSET,bool fSS=false)
		: queryOp(qop),ses(s),nReturn(nRet),values(vals),nValues(nV),mode(md&~(LOAD_CARDINALITY|LOAD_EXT_ADDR|LOAD_SSV)),stype(ste),op(sop),
		txid(INVALID_TXID),txcid(NO_TXCID),cnt(0),tx(s),fSnapshot(fSS) {}
	virtual				~Cursor();
	IPIN				*next();
	RC					next(Value&);
	RC					next(PID&);
	RC					next(IPIN *pins[],unsigned nPins,unsigned& nRet);
	RC					rewind();
	void				destroy();
};

};

#endif
