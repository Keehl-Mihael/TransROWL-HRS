#define REAL float
#define INT int

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <algorithm>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <map>
#include <fstream>
#include <list>
#include <set>
#include <vector>
using namespace std;

const REAL pi = 3.141592653589793238462643383;

INT threads = 12;
INT bernFlag = 1;
INT loadBinaryFlag = 0;
INT outBinaryFlag = 0;
INT trainTimes = 1000; //epoch - 1000
INT nbatches = 50;	   //batches - 50/100
INT dimension = 100;
REAL alpha = 0.001;
REAL margin = 1.0;

int failed = 0;
int tot_batches = 0;
int wrong = 0;

string inPath = "NELL_small/";
string outPath = "Embedded/";
string owlFolder = "OWLderived/";
string loadPath = "";
string note = "_OWL";

INT *lefHead, *rigHead;
INT *lefTail, *rigTail;

//OWL Variable
multimap<INT,INT> ent2class; 	//mapppa una entità nella classe di appartenzenza (se disponibile) id dbpedia
multimap<INT,INT> ent2cls;		//mappa una entità alla classe di appartenenza (per range e domain)
multimap<INT,INT> rel2range;		//mappa una relazione nel range, che corrisponde ad una classe
multimap<INT,INT> rel2domain;	//mappa una relazione nel domain, che corrisponde ad una classe
multimap<INT,INT> cls2false_t;	//data una entità, restituisce una classe falsa (per tail corruption)
list<int> functionalRel;
map<int,int> inverse;
map<int,int> equivalentRel;
//map<int,int> disjointWith;
multimap<int,int> equivalentClass;
map<int,int> entsubclass;
const string typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
int typeOf_id;

struct Triple {
	INT h, r, t;
};

Triple *trainHead, *trainTail, *trainList;

struct cmp_head {
	bool operator()(const Triple &a, const Triple &b) {
		return (a.h < b.h)||(a.h == b.h && a.r < b.r)||(a.h == b.h && a.r == b.r && a.t < b.t);
	}
};

struct cmp_tail {
	bool operator()(const Triple &a, const Triple &b) {
		return (a.t < b.t)||(a.t == b.t && a.r < b.r)||(a.t == b.t && a.r == b.r && a.h < b.h);
	}
};

/*
	There are some math functions for the program initialization.
*/
unsigned long long *next_random;

unsigned long long randd(INT id) {
	next_random[id] = next_random[id] * (unsigned long long)25214903917 + 11;
	return next_random[id];
}

INT rand_max(INT id, INT x) {
	INT res = randd(id) % x;
	while (res<0)
		res+=x;
	return res;
}

REAL rand(REAL min, REAL max) {
	return min + (max - min) * rand() / (RAND_MAX + 1.0);
}

REAL normal(REAL x, REAL miu,REAL sigma) {
	return 1.0/sqrt(2*pi)/sigma*exp(-1*(x-miu)*(x-miu)/(2*sigma*sigma));
}

REAL randn(REAL miu,REAL sigma, REAL min ,REAL max) {
	REAL x, y, dScope;
	do {
		x = rand(min,max);
		y = normal(x,miu,sigma);
		dScope=rand(0.0,normal(miu,miu,sigma));
	} while (dScope > y);
	return x;
}

void norm(REAL * con) {
	REAL x = 0;
	for (INT  ii = 0; ii < dimension; ii++)
		x += (*(con + ii)) * (*(con + ii));
	x = sqrt(x);
	if (x>1)
		for (INT ii=0; ii < dimension; ii++)
			*(con + ii) /= x;
}

//vero se l'entità index è di classe class_id
bool inRange(int id_rel, int id_obj) {
    pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	//prendi le classi di id_obj!!
	ret = ent2cls.equal_range(id_obj);
	//prendi le classi di id_obj!!
	set<INT> cls;
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it) {
		cls.insert(it->second);
	}
	ret = rel2range.equal_range(id_rel);
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it)
		if(cls.find(it->second) != cls.end())
			return true;

	return false;
}

bool inDomain(int id_rel, int id_sub) {
    pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	//prendi le classi di id_obj!!
	ret = ent2cls.equal_range(id_sub);
	//prendi le classi di id_obj!!
	set<INT> cls;
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it) {
		cls.insert(it->second);
	}
	ret = rel2domain.equal_range(id_rel);
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it)
		if(cls.find(it->second) != cls.end())
			return true;

	return false;
}

//vero se l'entità index è di classe class_id
bool containClass(int class_id, int index) {
    pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	ret = ent2class.equal_range(index);
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it)
		if(it->second == class_id)
			return true;
	return false;
}

//vero se la relazione è di tipo functional
bool isFunctional(int rel_id) {
	if(functionalRel.size() == 0)
		return false;
	for(list<int>::iterator it = functionalRel.begin(); it != functionalRel.end(); ++it)
		if( (*it) == rel_id)
			return true;
	return false;
}

int getInverse(int rel) {
	if(inverse.size() == 0)
		return -1;
	if(inverse.find(rel) != inverse.end())
		return inverse.find(rel)->second;
	else
		return -1;
}

int getEquivalentProperty(int rel) {
	if(equivalentRel.size() == 0)
		return -1;
	if(equivalentRel.find(rel) != equivalentRel.end())
		return equivalentRel.find(rel)->second;
	else
		return -1;
}

set<int> getEquivalentClass(int id_class) {
	set<int> cls;
	pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	ret = equivalentClass.equal_range(id_class);
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it)
		cls.insert(it->second);
	return cls;
}


void getSubclass() {
	multimap<int,int> sub2superclass; //mappa id in id della superclasse
	ifstream subclass_file(inPath + "subclassID.txt");
	string tmp;
	while(getline(subclass_file,tmp)) {
		string::size_type pos=tmp.find('\t',0);
		int id_subclass= atoi(tmp.substr(0,pos).c_str());
		int id_superclass = atoi(tmp.substr(pos+1).c_str());
		sub2superclass.insert(pair<int,int>(id_subclass,id_superclass));
	}
	subclass_file.close();

	ifstream todbp_file(inPath + "todbpedia.txt");
	map<int,int> ent2cls;
	map<int,int> cls2ent;
	while(getline(todbp_file,tmp)) {
		string::size_type pos=tmp.find('\t',0);
		int entity = atoi(tmp.substr(0,pos).c_str());
		int dbp_class = atoi(tmp.substr(pos+1).c_str());
		cls2ent.insert(pair<int,int>(dbp_class, entity));
		ent2cls.insert(pair<int,int>(entity,dbp_class));
	}
	todbp_file.close();

	for(map<int,int>::iterator it = ent2cls.begin(); it != ent2cls.end(); ++it) {
		int entity = it->first;
		int dbp_class = it->second;
		pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
		ret = sub2superclass.equal_range(dbp_class); //restituisce tutti i valori con chiave=id_dbpedia
		for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it) {
			if(cls2ent.find(it->second) != cls2ent.end())
				entsubclass.insert(pair<int,int>(entity,cls2ent.find(it->second)->second));
		}
	}
}

// read owl information from files prepared using DBPedia OWL (see dbpediaUtils)
void owlInit() {
    string tmp;
    map<string,int> rel2id;
    map<string,int> ent2id;

	printf("Loading ontology information...\n");

	//carico le relazioni per il confronto
    ifstream rel_file(inPath + "relation2id.txt");
    getline(rel_file,tmp);
    while (getline(rel_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
        string rel = tmp.substr(0,pos);
        int id = atoi(tmp.substr(pos+1).c_str());
        rel2id.insert(pair<string,int>(rel,id));
        if(rel == typeOf)
        	typeOf_id = id;
    }
    printf("ID TypeOf: %d\n",typeOf_id);
    rel_file.close();

	//carico le entità per il confronto
    ifstream ent_file(inPath + "entity2id.txt");
    getline(ent_file,tmp);
    while (getline(ent_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
        string ent = tmp.substr(0,pos);
        int id = atoi(tmp.substr(pos+1).c_str());
        ent2id.insert(pair<string,int>(ent,id));
    }
    ent_file.close();

    ifstream class_file(inPath + "entity2class.txt");
    while (getline(class_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
    	int entity= atoi(tmp.substr(0,pos).c_str());
    	int class_id = atoi(tmp.substr(pos+1).c_str());
    	ent2class.insert(pair<int,int>(entity,class_id));
    }
    class_file.close();

    ifstream domain_file(inPath + "rs_domain2id.txt");
    while (getline(domain_file, tmp)) {
        string::size_type pos=tmp.find(' ',0);
    	int relation= atoi(tmp.substr(0,pos).c_str());
    	int domain = atoi(tmp.substr(pos+1).c_str());
    	rel2domain.insert(pair<int,int>(relation,domain));
    }
    domain_file.close();

    ifstream range_file(inPath + "rs_range2id.txt");
    while (getline(range_file, tmp)) {
        string::size_type pos=tmp.find(' ',0);
    	int relation= atoi(tmp.substr(0,pos).c_str());
    	int range = atoi(tmp.substr(pos+1).c_str());
    	rel2range.insert(pair<int,int>(relation,range));
    }
    range_file.close();

    ifstream false_file(inPath + "falseTypeOf2id.txt");
	while (getline(false_file, tmp)) {
		string::size_type pos = tmp.find(' ',0);
		string last_part= tmp.substr(pos+1);
		int head = atoi(tmp.substr(0,pos).c_str());
		pos=last_part.find(' ',0);
		int cls = atoi(last_part.substr(0,pos).c_str());
		cls2false_t.insert(pair<int,int>(head,cls));
		//cls2false_h.insert(pair<int,int>(cls,head));
	}

    //trovo le relazioni di tipo functional
    ifstream function_file(inPath + owlFolder + "functionalProperty.txt");
    while (getline(function_file, tmp)) {
        if (rel2id.find(tmp) != rel2id.end()){
            functionalRel.push_front(rel2id.find(tmp)->second);
        }
    }
    function_file.close();

    //trovo le relazioni inverseOf
    ifstream inverse_file(inPath + owlFolder + "inverseOf.txt");
    while (getline(inverse_file, tmp)) {
    	string::size_type pos=tmp.find(' ',0);
		string first = tmp.substr(0,pos);
		if(rel2id.find(first) != rel2id.end()) {
			string second= tmp.substr(pos+1);
			second = second.substr(0, second.length());
			if(rel2id.find(second) != rel2id.end()) {
				int id_first = rel2id.find(first)->second;
				int id_second = rel2id.find(second)->second;
				inverse.insert(pair<int,int>(id_first, id_second));
				inverse.insert(pair<int,int>(id_second,id_first));
			}
		}
    }
    inverse_file.close();

    ifstream eqProp_file(inPath + owlFolder + "equivalentProperty.txt");
    while (getline(eqProp_file, tmp)) {
    	string::size_type pos=tmp.find(' ',0);
		string first = tmp.substr(0,pos);
		if(rel2id.find(first) != rel2id.end()) {
			string second= tmp.substr(pos+1);
			second = second.substr(0, second.length());
			if(rel2id.find(second) != rel2id.end()) {
				int id_first = rel2id.find(first)->second;
				int id_second = rel2id.find(second)->second;
				equivalentRel.insert(pair<int,int>(id_first, id_second));
				equivalentRel.insert(pair<int,int>(id_second,id_first));
			}
		}
    }
    eqProp_file.close();

    ifstream eqClass_file(inPath + owlFolder + "equivalentClass.txt");
    while (getline(eqClass_file, tmp)) {
    	string::size_type pos=tmp.find(' ',0);
		string first = tmp.substr(0,pos);
		if(ent2id.find(first) != ent2id.end()) {
			string second= tmp.substr(pos+1);
			second = second.substr(0, second.length());
			if(ent2id.find(second) != ent2id.end()) {
				int id_first = ent2id.find(first)->second;
				int id_second = ent2id.find(second)->second;
				equivalentClass.insert(pair<int,int>(id_first, id_second));
				equivalentClass.insert(pair<int,int>(id_second,id_first));
			}
		}
    }
    eqClass_file.close();

    multimap<int,int> dbp_disj;
    ifstream disj_file(inPath + "disjoint2id.txt");
    while (getline(disj_file, tmp)) {
    	string::size_type pos=tmp.find('\t',0);
    	int cls= atoi(tmp.substr(0,pos).c_str());
		int disjoint = atoi(tmp.substr(pos+1).c_str());
		dbp_disj.insert(pair<int,int>(cls,disjoint));
    }
    disj_file.close();

    getSubclass();

}

/*
	Read triples from the training file.
*/

INT relationTotal, entityTotal, tripleTotal;
REAL *relationVec, *entityVec;
REAL *relationVecDao, *entityVecDao;
INT *freqRel, *freqEnt;
REAL *left_mean, *right_mean;

void init() {

	FILE *fin;
	INT tmp;

	fin = fopen((inPath + "relation2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &relationTotal);
	fclose(fin);

	relationVec = (REAL *)calloc(relationTotal * dimension, sizeof(REAL));
	for (INT i = 0; i < relationTotal; i++) {
		for (INT ii=0; ii<dimension; ii++)
			relationVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
	}

	fin = fopen((inPath + "entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &entityTotal);
	fclose(fin);

	entityVec = (REAL *)calloc(entityTotal * dimension, sizeof(REAL));
	for (INT i = 0; i < entityTotal; i++) {
		for (INT ii=0; ii<dimension; ii++)
			entityVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
		norm(entityVec+i*dimension);
	}

	freqRel = (INT *)calloc(relationTotal + entityTotal, sizeof(INT));
	freqEnt = freqRel + relationTotal;

	fin = fopen((inPath + "train2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &tripleTotal);
	trainHead = (Triple *)calloc(tripleTotal, sizeof(Triple));
	trainTail = (Triple *)calloc(tripleTotal, sizeof(Triple));
	trainList = (Triple *)calloc(tripleTotal, sizeof(Triple));
	for (INT i = 0; i < tripleTotal; i++) {
		tmp = fscanf(fin, "%d", &trainList[i].h);
		tmp = fscanf(fin, "%d", &trainList[i].t);
		tmp = fscanf(fin, "%d", &trainList[i].r);
		if(trainList[i].r == typeOf_id) {
			ent2cls.insert(pair<int,int>(trainList[i].h,trainList[i].t));
		}
		freqEnt[trainList[i].t]++;
		freqEnt[trainList[i].h]++;
		freqRel[trainList[i].r]++;
		trainHead[i] = trainList[i];
		trainTail[i] = trainList[i];
	}
	fclose(fin);

	sort(trainHead, trainHead + tripleTotal, cmp_head());
	sort(trainTail, trainTail + tripleTotal, cmp_tail());

	lefHead = (INT *)calloc(entityTotal, sizeof(INT));
	rigHead = (INT *)calloc(entityTotal, sizeof(INT));
	lefTail = (INT *)calloc(entityTotal, sizeof(INT));
	rigTail = (INT *)calloc(entityTotal, sizeof(INT));
	memset(rigHead, -1, sizeof(INT)*entityTotal);
	memset(rigTail, -1, sizeof(INT)*entityTotal);
	for (INT i = 1; i < tripleTotal; i++) {
		if (trainTail[i].t != trainTail[i - 1].t) {
			rigTail[trainTail[i - 1].t] = i - 1;
			lefTail[trainTail[i].t] = i;
		}
		if (trainHead[i].h != trainHead[i - 1].h) {
			rigHead[trainHead[i - 1].h] = i - 1;
			lefHead[trainHead[i].h] = i;
		}
	}
	rigHead[trainHead[tripleTotal - 1].h] = tripleTotal - 1;
	rigTail[trainTail[tripleTotal - 1].t] = tripleTotal - 1;

	left_mean = (REAL *)calloc(relationTotal * 2, sizeof(REAL));
	right_mean = left_mean + relationTotal;
	for (INT i = 0; i < entityTotal; i++) {
		for (INT j = lefHead[i] + 1; j <= rigHead[i]; j++)
			if (trainHead[j].r != trainHead[j - 1].r)
				left_mean[trainHead[j].r] += 1.0;
		if (lefHead[i] <= rigHead[i])
			left_mean[trainHead[lefHead[i]].r] += 1.0;
		for (INT j = lefTail[i] + 1; j <= rigTail[i]; j++)
			if (trainTail[j].r != trainTail[j - 1].r)
				right_mean[trainTail[j].r] += 1.0;
		if (lefTail[i] <= rigTail[i])
			right_mean[trainTail[lefTail[i]].r] += 1.0;
	}

	for (INT i = 0; i < relationTotal; i++) {
		left_mean[i] = freqRel[i] / left_mean[i];
		right_mean[i] = freqRel[i] / right_mean[i];
	}

	relationVecDao = (REAL*)calloc(dimension * relationTotal, sizeof(REAL));
	entityVecDao = (REAL*)calloc(dimension * entityTotal, sizeof(REAL));
}

void load_binary() {
	struct stat statbuf1;
	if (stat((loadPath + "entity2vec" + note + ".bin").c_str(), &statbuf1) != -1) {
		INT fd = open((loadPath + "entity2vec" + note + ".bin").c_str(), O_RDONLY);
		REAL* entityVecTmp = (REAL*)mmap(NULL, statbuf1.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		memcpy(entityVec, entityVecTmp, statbuf1.st_size);
		munmap(entityVecTmp, statbuf1.st_size);
		close(fd);
	}
	struct stat statbuf2;
	if (stat((loadPath + "relation2vec" + note + ".bin").c_str(), &statbuf2) != -1) {
		INT fd = open((loadPath + "relation2vec" + note + ".bin").c_str(), O_RDONLY);
		REAL* relationVecTmp =(REAL*)mmap(NULL, statbuf2.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		memcpy(relationVec, relationVecTmp, statbuf2.st_size);
		munmap(relationVecTmp, statbuf2.st_size);
		close(fd);
	}
}

void load() {
	if (loadBinaryFlag) {
		load_binary();
		return;
	}
	FILE *fin;
	INT tmp;
	fin = fopen((loadPath + "entity2vec" + note + ".vec").c_str(), "r");
	for (INT i = 0; i < entityTotal; i++) {
		INT last = i * dimension;
		for (INT j = 0; j < dimension; j++)
			tmp = fscanf(fin, "%f", &entityVec[last + j]);
	}
	fclose(fin);
	fin = fopen((loadPath + "relation2vec" + note + ".vec").c_str(), "r");
	for (INT i = 0; i < relationTotal; i++) {
		INT last = i * dimension;
		for (INT j = 0; j < dimension; j++)
			tmp = fscanf(fin, "%f", &relationVec[last + j]);
	}
	fclose(fin);
}


/*
	Training process of transE.
*/

INT Len;
INT Batch;
REAL res;

INT corrupt_head(INT id, INT h, INT r) {
	INT lef, rig, mid, ll, rr;
	lef = lefHead[h] - 1;
	rig = rigHead[h];
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].r >= r) rig = mid; else
		lef = mid;
	}
	ll = rig;
	lef = lefHead[h];
	rig = rigHead[h] + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].r <= r) lef = mid; else
		rig = mid;
	}
	rr = lef;
	INT tmp = rand_max(id, entityTotal - (rr - ll + 1));
	if (tmp < trainHead[ll].t) return tmp;
	if (tmp > trainHead[rr].t - rr + ll - 1) return tmp + rr - ll + 1;
	lef = ll, rig = rr + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainHead[mid].t - mid + ll - 1 < tmp)
			lef = mid;
		else
			rig = mid;
	}
	return tmp + lef - ll + 1;
}

INT corrupt_tail(INT id, INT t, INT r) {
	INT lef, rig, mid, ll, rr;
	lef = lefTail[t] - 1;
	rig = rigTail[t];
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].r >= r) rig = mid; else
		lef = mid;
	}
	ll = rig;
	lef = lefTail[t];
	rig = rigTail[t] + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].r <= r) lef = mid; else
		rig = mid;
	}
	rr = lef;
	INT tmp = rand_max(id, entityTotal - (rr - ll + 1));
	if (tmp < trainTail[ll].h) return tmp;
	if (tmp > trainTail[rr].h - rr + ll - 1) return tmp + rr - ll + 1;
	lef = ll, rig = rr + 1;
	while (lef + 1 < rig) {
		mid = (lef + rig) >> 1;
		if (trainTail[mid].h - mid + ll - 1 < tmp)
			lef = mid;
		else
			rig = mid;
	}
	return tmp + lef - ll + 1;
}

REAL calc_sum(INT e1, INT e2, INT rel) {
	REAL sum=0;
	INT last1 = e1 * dimension;
	INT last2 = e2 * dimension;
	INT lastr = rel * dimension;
	for (INT ii=0; ii < dimension; ii++)
		sum += fabs(entityVec[last2 + ii] - entityVec[last1 + ii] - relationVec[lastr + ii]);
	return sum;
}

void gradient(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimension;
	INT lastb1 = e1_b * dimension;
	INT lastb2 = e2_b * dimension;
	INT lastbr = rel_b * dimension;
	//se entità equivalenti, salva array
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = (entityVec[lasta2 + ii] - entityVec[lasta1 + ii] - relationVec[lastar + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;
		relationVec[lastar + ii] -= x;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;
		x = (entityVec[lastb2 + ii] - entityVec[lastb1 + ii] - relationVec[lastbr + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		relationVec[lastbr + ii] -=  x;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;
	}
}

void gradientSubClass(INT cls1_a, INT cls2_a, INT cls1_b, INT cls2_b) {
	INT lasta1 = cls1_a * dimension;
	INT lasta2 = cls2_a * dimension;
	INT lastb1 = cls1_b * dimension;
	INT lastb2 = cls2_b * dimension;
	//se entità equivalenti, salva array
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = 2 * (entityVec[lasta2 + ii] - entityVec[lasta1 + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;
		x = 2 * (entityVec[lastb2 + ii] - entityVec[lastb1 + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;
	}
}

void gradientInverseOf(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, INT inverseRel) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimension;
	INT lastb1 = e1_b * dimension;
	INT lastb2 = e2_b * dimension;
	INT lastbr = rel_b * dimension;
	INT lastInverse = inverseRel * dimension;
	//se entità equivalenti, salva array
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = (entityVec[lasta2 + ii] - entityVec[lasta1 + ii] - relationVec[lastar + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;

		relationVec[lastar + ii] -= x;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;

		relationVec[lastInverse + ii] -= x;
		entityVec[lasta2 + ii] -= x;
		entityVec[lasta1 + ii] += x;

		x = (entityVec[lastb2 + ii] - entityVec[lastb1 + ii] - relationVec[lastbr + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		relationVec[lastbr + ii] -=  x;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;

		relationVec[lastInverse + ii] -=  x;
		entityVec[lastb2 + ii] -= x;
		entityVec[lastb1 + ii] += x;
	}
}

void gradientEquivalentProperty(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, INT equivalent) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimension;
	INT lastb1 = e1_b * dimension;
	INT lastb2 = e2_b * dimension;
	INT lastbr = rel_b * dimension;
	INT lastEquivalent = equivalent * dimension;
	//se entità equivalenti, salva array
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = (entityVec[lasta2 + ii] - entityVec[lasta1 + ii] - relationVec[lastar + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;
		relationVec[lastar + ii] -= x;
		relationVec[lastEquivalent + ii] -= x;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;
		x = (entityVec[lastb2 + ii] - entityVec[lastb1 + ii] - relationVec[lastbr + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		relationVec[lastbr + ii] -=  x;
		relationVec[lastEquivalent + ii] -=  x;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;
	}
}


int getFalseClass(int entity) {
	int j = -1;
	pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	ret = cls2false_t.equal_range(entity);
	vector<INT> cls_vec;
	for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it) {
		cls_vec.push_back(it->second);
	}
	if(cls_vec.size() > 0) {
		int RandIndex = rand() % cls_vec.size();
		j = cls_vec[RandIndex];
	}
	return j;
}

void gradientEquivalentClass(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, INT id) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimension;
	INT lastb1 = e1_b * dimension;
	INT lastb2 = e2_b * dimension;
	INT lastbr = rel_b * dimension;
	vector<int> corr_tails;
	vector<int> corr_heads;
	set<int> eq_cls = getEquivalentClass(e2_a);


	for(set<int>::iterator it = eq_cls.begin(); it != eq_cls.end(); ++it) {
		int tail = getFalseClass(e1_a);
		if(tail == -1)
			tail = corrupt_head(id, e1_a, rel_a);
		int head = corrupt_tail(id, e2_a, rel_a);
		corr_tails.push_back(tail);
		corr_heads.push_back(head);
	}

	//se entità equivalenti, salva array
	for (INT ii=0; ii  < dimension; ii++) {
		REAL x;
		x = (entityVec[lasta2 + ii] - entityVec[lasta1 + ii] - relationVec[lastar + ii]);
		if (x > 0)
			x = -alpha;
		else
			x = alpha;
		int pos_x = x;
		relationVec[lastar + ii] -= x;
		entityVec[lasta1 + ii] -= x;
		entityVec[lasta2 + ii] += x;
		x = (entityVec[lastb2 + ii] - entityVec[lastb1 + ii] - relationVec[lastbr + ii]);
		if (x > 0)
			x = alpha;
		else
			x = -alpha;
		int neg_x = x;
		relationVec[lastbr + ii] -=  x;
		entityVec[lastb1 + ii] -= x;
		entityVec[lastb2 + ii] += x;

		if(eq_cls.size() > 0) {
			int i = 0;
			for(set<int>::iterator it = eq_cls.begin(); it != eq_cls.end(); ++it) {
				INT lastCls = (*it) * dimension;
				relationVec[lastar + ii] -= pos_x;
				entityVec[lasta1 + ii] -= pos_x;
				entityVec[lastCls + ii] += pos_x;

				uint pr;
				if (bernFlag)
					pr = 1000 * right_mean[rel_a] / (right_mean[rel_a] + left_mean[rel_a]);
				else
					pr = 500;

				if (randd(id) % 1000 < pr) {
					int tail = corr_tails[i];
					INT last_tail = tail * dimension;
					relationVec[lastbr + ii] -=  neg_x;
					entityVec[lastb1 + ii] -= neg_x;
					entityVec[last_tail + ii] += neg_x;
				} else {
					int head = corr_heads[i];
					INT last_head = head * dimension;
					relationVec[lastbr + ii] -=  neg_x;
					entityVec[last_head + ii] -= neg_x;
					entityVec[lastb2 + ii] += neg_x;
				}
				i++;
			}
		}
	}
}

void train_kb(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, INT id) {
	//e1_a,rel_a,e2_a è una tupla presente in train2id.txt
	REAL sum1 = calc_sum(e1_a, e2_a, rel_a);
	REAL sum2 = calc_sum(e1_b, e2_b, rel_b);
	if (sum1 + margin > sum2) {
		res += margin + sum1 - sum2;
		if(rel_a == typeOf_id) {
			gradientEquivalentClass(e1_a, e2_a, rel_a, e1_b, e2_b, rel_b,id);
		} else if(getInverse(rel_a) != -1) {
			gradientInverseOf(e1_a, e2_a, rel_a, e1_b, e2_b, rel_b, getInverse(rel_a));
		} else if(getEquivalentProperty(rel_a) != -1) {
			gradientEquivalentProperty(e1_a, e2_a, rel_a, e1_b, e2_b, rel_b, getEquivalentProperty(rel_a));
		} else {
			gradient(e1_a, e2_a, rel_a, e1_b, e2_b, rel_b);
		}
		if(rel_a == typeOf_id && entsubclass.find(e2_a) != entsubclass.end()) {
			gradientSubClass(e2_a, entsubclass.find(e2_a)->second, e2_a, e2_b);
		}
	}
}

int getHeadCorrupted(int triple_index, int id) {
	int j = -1;
	//Formula di Slovin per la dimensione del campione (generati da corrupt_tail)
	float error = 0.2f; //margine di errore del 20%
	int tries = entityTotal / (1+ entityTotal*error*error);
	for(int i = 0; i < tries; i++) {
		int corrpt_head = corrupt_tail(id, trainList[triple_index].t, trainList[triple_index].r);
		if(!inDomain(trainList[triple_index].r, corrpt_head)){
			j = corrpt_head;
			break;
		}
	}
	return j;
}

/*	Ottieni una coda corrotta
 *	Se la classe della coda non rispetta il range
 *		sceglila per l'addestramento
 *	altrimenti scegli un'altra coda
 *	ripeti per n tentativi, prima di rinunciare e andare col metodo standard
 */
int getTailCorrupted(int triple_index, int id) {
	int j = -1;
	//Formula di Slovin per la dimensione del campione (generati da corrupt_tail)
	float error = 0.2f; //margine di errore del 20%
	int tries = entityTotal / (1+ entityTotal*error*error);
	for(int i = 0; i < tries; i++) {
		int corrpt_tail = corrupt_head(id, trainList[triple_index].h, trainList[triple_index].r);
		if(!inRange(trainList[triple_index].r, corrpt_tail)){
			j = corrpt_tail;
			break;
		}
	}
	return j;
}


bool hasRange(int rel) {
	pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	ret = rel2range.equal_range(rel);
	return (rel2range.count(rel) > 0);
}

bool hasDomain(int rel) {
	pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
	ret = rel2range.equal_range(rel);
	return (rel2domain.count(rel) > 0);
}

//scegli in modo casuale una coda o una testa da corrompere e addestra
int train(int triple_index, int id,int range, int domain) {
	int j = -1;
	uint pr;
	if (bernFlag)
		pr = 1000 * right_mean[trainList[triple_index].r] / (right_mean[trainList[triple_index].r] + left_mean[trainList[triple_index].r]);
	else
		pr = 500;
	if (randd(id) % 1000 < pr || isFunctional(trainList[triple_index].r)) {
//		if(trainList[triple_index].r == typeOf_id) {
//			j = getFalseClass(trainList[triple_index].h);
//		} else
		if(hasRange(trainList[triple_index].r)) {
			j = getTailCorrupted(triple_index, id);
		}
		if(j == -1)
			j = corrupt_head(id, trainList[triple_index].h, trainList[triple_index].r);
		train_kb(trainList[triple_index].h, trainList[triple_index].t, trainList[triple_index].r, trainList[triple_index].h, j, trainList[triple_index].r,id);
	} else {
		if(hasDomain(trainList[triple_index].r))
			j = getHeadCorrupted(triple_index, id);
		if(j == -1)
			j = corrupt_tail(id, trainList[triple_index].t, trainList[triple_index].r);

		train_kb(trainList[triple_index].h, trainList[triple_index].t, trainList[triple_index].r, j, trainList[triple_index].t, trainList[triple_index].r,id);
	}
	tot_batches++;
	return j;
}

void* trainMode(void *con) {
	INT id, index, j;
	id = (unsigned long long)(con);
	next_random[id] = rand();
	for (INT k = Batch / threads; k >= 0; k--) {
		index = rand_max(id, Len); //ottieni un indice casuale

		//otteni il dominio della relazione e la classe della entità testa che dovrà rispettarlo
		int domain = -1;
		if(rel2domain.find(trainList[index].r) != rel2domain.end()){
			domain = rel2domain.find(trainList[index].r)->second;
		}
		//otteni il codominio (range) della relazione e la classe della entità coda che dovrà rispettarlo
		int range = -1;
		if(rel2range.find(trainList[index].r) != rel2range.end()){
			range = rel2range.find(trainList[index].r)->second;
		}
		j = train(index,id,range,domain);
		norm(relationVec + dimension * trainList[index].r);
		norm(entityVec + dimension * trainList[index].h);
		norm(entityVec + dimension * trainList[index].t);
		norm(entityVec + dimension * j);
	}
	pthread_exit(NULL);
}

void train(void *con) {
	srand(time(0));

	Len = tripleTotal;
	Batch = Len / nbatches;
	next_random = (unsigned long long *)calloc(threads, sizeof(unsigned long long));
	for (INT epoch = 0; epoch < trainTimes; epoch++) {
		res = 0;
		for (INT batch = 0; batch < nbatches; batch++) {
			pthread_t *pt = (pthread_t *)malloc(threads * sizeof(pthread_t));
			for (long a = 0; a < threads; a++)
				pthread_create(&pt[a], NULL, trainMode,  (void*)a);
			for (long a = 0; a < threads; a++)
				pthread_join(pt[a], NULL);
			free(pt);
		}
		printf("epoch %d %f %d\n", epoch, res,trainTimes);
	}
}

/*
	Get the results of transE.
*/

void out_binary() {
		INT len, tot;
		REAL *head;
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".bin").c_str(), "wb");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".bin").c_str(), "wb");
		len = relationTotal * dimension; tot = 0;
		head = relationVec;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f2);
			tot = tot + sum;
		}
		len = entityTotal * dimension; tot = 0;
		head = entityVec;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f3);
			tot = tot + sum;
		}
		fclose(f2);
		fclose(f3);
}

void out() {
		if (outBinaryFlag) {
			out_binary();
			return;
		}
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".vec").c_str(), "w");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".vec").c_str(), "w");
		for (INT i=0; i < relationTotal; i++) {
			INT last = dimension * i;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f2, "%.6f\t", relationVec[last + ii]);
			fprintf(f2,"\n");
		}
		for (INT  i = 0; i < entityTotal; i++) {
			INT last = i * dimension;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f3, "%.6f\t", entityVec[last + ii] );
			fprintf(f3,"\n");
		}
		fclose(f2);
		fclose(f3);
}

void CSVout() {
		if (outBinaryFlag) {
			out_binary();
			return;
		}
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".csv").c_str(), "w");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".csv").c_str(), "w");
		for (INT i=0; i < relationTotal; i++) {
			INT last = dimension * i;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f2, "%.6f,", relationVec[last + ii]);
			fprintf(f2,"\n");
		}
		for (INT  i = 0; i < entityTotal; i++) {
			INT last = i * dimension;
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f3, "%.6f,", entityVec[last + ii] );
			fprintf(f3,"\n");
		}
		fclose(f2);
		fclose(f3);
}

/*
	Main function
*/

int ArgPos(char *str, int argc, char **argv) {
	int a;
	for (a = 1; a < argc; a++) if (!strcmp(str, argv[a])) {
		if (a == argc - 1) {
			printf("Argument missing for %s\n", str);
			exit(1);
		}
		return a;
	}
	return -1;
}


void setparameters(int argc, char **argv) {
	int i;
	if ((i = ArgPos((char *)"-size", argc, argv)) > 0) dimension = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-input", argc, argv)) > 0) inPath = argv[i + 1];
	if ((i = ArgPos((char *)"-output", argc, argv)) > 0) outPath = argv[i + 1];
	if ((i = ArgPos((char *)"-load", argc, argv)) > 0) loadPath = argv[i + 1];
	if ((i = ArgPos((char *)"-thread", argc, argv)) > 0) threads = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-epochs", argc, argv)) > 0) trainTimes = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-nbatches", argc, argv)) > 0) nbatches = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-alpha", argc, argv)) > 0) alpha = atof(argv[i + 1]);
	if ((i = ArgPos((char *)"-margin", argc, argv)) > 0) margin = atof(argv[i + 1]);
	if ((i = ArgPos((char *)"-load-binary", argc, argv)) > 0) loadBinaryFlag = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-out-binary", argc, argv)) > 0) outBinaryFlag = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-note", argc, argv)) > 0) note = argv[i + 1];
}

int main(int argc, char **argv) {
	setparameters(argc, argv);
	printf("Prepare\n");
	init();
	owlInit();
	if (loadPath != "") load();
	printf("Train\n");
	train(NULL);
	if (outPath != "") out();
	return 0;
}

