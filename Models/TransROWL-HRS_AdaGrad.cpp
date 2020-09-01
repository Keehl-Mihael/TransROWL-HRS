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
#include <sstream>


using namespace std;

const REAL pi = 3.141592653589793238462643383;

INT threads = 12;
INT bernFlag = 1;
INT loadBinaryFlag = 0;
INT outBinaryFlag = 0;
INT trainTimes = 250;
INT nbatches = 50;
INT dimension = 100;
INT dimensionR = 100;
REAL alpha = 0.001;
REAL lambdaC= 0.00001;
REAL lambdaR= 0.0001;
REAL epsilon = 1;
REAL epsilonC = 1 + lambdaC;
REAL epsilonR = 1 + lambdaR;
REAL limit=100000000;
REAL margin = 1;

int failed = 0;
int tot_batches = 0;
int wrong = 0;
INT epoch;
INT epoch_start=100;

string inPath = "NELL_small/";
string outPath = "Embedded/";
string owlFolder = "OWLderived/";
string loadPath = "";
string initPath = "NELL_small/";
string note = "_ROWL-HRS_AdaGrad";
string note1 = "_E";

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

vector<vector<int>> cluster2rel;

struct Triple {
	INT h, r, t;
};

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

struct cmp_list {
	INT minimal(INT a,INT b) {
		if (a < b) return b;
		return a;
	}
	bool operator()(const Triple &a, const Triple &b) {
		return (minimal(a.h, a.t) < minimal(b.h, b.t));
	}
};

Triple *trainHead, *trainTail, *trainList;

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

void norm(REAL *con, INT dimension) {
	REAL x = 0;
	for (INT  ii = 0; ii < dimension; ii++)
		x += (*(con + ii)) * (*(con + ii));
	x = sqrt(x);
	if (x>1)
		for (INT ii=0; ii < dimension; ii++)
			*(con + ii) /= x;
}

void norm(REAL *con, REAL *matrix) {
	REAL tmp, x;
	INT last;
	x = 0;
	last = 0;
	for (INT ii = 0; ii < dimensionR; ii++) {
		tmp = 0;
		for (INT jj=0; jj < dimension; jj++) {
			tmp += matrix[last] * con[jj];
			last++;
		}
		x += tmp * tmp;
	}
	if (x>1) {
		REAL lambda = 1;
		for (INT ii = 0, last = 0; ii < dimensionR; ii++, last += dimension) {
			tmp = 0;
			for (INT jj = 0; jj < dimension; jj++)
				tmp += ((matrix[last + jj] * con[jj]) * 2);
			for (INT jj = 0; jj < dimension; jj++) {
				matrix[last + jj] -= alpha * lambda * tmp * con[jj];
				con[jj] -= alpha * lambda * tmp * matrix[last + jj];
			}
		}
	}
}

INT relationTotal, entityTotal, tripleTotal;
INT *freqRel, *freqEnt;
REAL *left_mean, *right_mean;
REAL *relationVec,*relationCVec, *entityVec, *matrix,*relationVecUpdate,*relationCVecUpdate, *entityVecUpdate, *matrixUpdate;
REAL *relationVecDao,*relationCVecDao, *entityVecDao, *matrixDao;
REAL *tmpValue;

void norm(INT h, INT t, INT r, INT j) {
		norm(relationVecDao + dimensionR * r, dimensionR);
		norm(entityVecDao + dimension * h, dimension);
		norm(entityVecDao + dimension * t, dimension);
		norm(entityVecDao + dimension * j, dimension);
		norm(entityVecDao + dimension * h, matrixDao + dimension * dimensionR * r);
		norm(entityVecDao + dimension * t, matrixDao + dimension * dimensionR * r);
		norm(entityVecDao + dimension * j, matrixDao + dimension * dimensionR * r);
}

/*
	Read triples from the training file.
*/

void readingclusters(){
    ifstream filn("clusters.txt");
    int i=0;
    string line;
    while(getline( filn, line )){
        size_t found = line.find(' ',0);
        vector<int> vec;
        cluster2rel.push_back(vec);
        stringstream is(line);
        int va;
        while(is >> va){
            //printf("%d\t\t",found);
            //int val=atoi(va);
            cluster2rel[i].push_back(va);
            //found=found+1;
        }

        cout<< i+1 <<" : ";
        for(auto j = cluster2rel[i].begin(); j != cluster2rel[i].end(); ++j)
            std::cout << *j<<' ';
        cout<<endl;
        i++;

    }
    printf("%d\n",cluster2rel.size());
    filn.close();
}

int findcluster(int relation){
    for(int i=0;i<cluster2rel.size();i++){
        for(auto j = cluster2rel[i].begin(); j != cluster2rel[i].end(); ++j){
            if(*j==relation){
                return i;
            }
        }
    }
    return -1;
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
    while (getline(function_file, tmp))
        if(rel2id.find(tmp) != rel2id.end())
            functionalRel.push_front(rel2id.find(tmp)->second);
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


void init() {

	FILE *fin;
	INT tmp;

	fin = fopen((inPath + "relation2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &relationTotal);
	fclose(fin);

	fin = fopen((inPath + "entity2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &entityTotal);
	fclose(fin);

	relationVec = (REAL *)calloc(relationTotal * dimensionR * 2 + entityTotal * dimension * 2 + relationTotal * dimension * dimensionR * 2, sizeof(REAL));
	relationVecDao = relationVec + relationTotal * dimensionR;
	entityVec = relationVecDao + relationTotal * dimensionR;
	entityVecDao = entityVec + entityTotal * dimension;
	matrix = entityVecDao + entityTotal * dimension;
	matrixDao = matrix + dimension * dimensionR * relationTotal;
	freqRel = (INT *)calloc(relationTotal + entityTotal, sizeof(INT));
	freqEnt = freqRel + relationTotal;

    relationCVec = (REAL *)calloc(((int)cluster2rel.size()) * dimensionR, sizeof(REAL));
    for (INT i = 0; i < ((int)cluster2rel.size()); i++) {
        for (INT ii=0; ii<dimension; ii++)
            relationCVec[i * dimension + ii] = 0;
    }

    relationCVecUpdate = (REAL *)calloc(((int)cluster2rel.size()*dimensionR), sizeof(REAL));
    for (INT i = 0; i < ((int)cluster2rel.size()); i++) {
        for (INT ii=0; ii<dimension; ii++)
            relationCVecUpdate[i * dimension + ii] = epsilonC*epsilonC;
    }

    relationVecUpdate = (REAL *)calloc(relationTotal * dimensionR, sizeof(REAL));
    for (INT i = 0; i < relationTotal; i++) {
        for (INT ii=0; ii<dimensionR; ii++)
            relationVecUpdate[i * dimension + ii] = epsilonR*epsilonR;
    }

	for (INT i = 0; i < relationTotal; i++) {
		for (INT ii=0; ii < dimensionR; ii++)
			relationVec[i * dimensionR + ii] = randn(0, 1.0 / dimensionR, -6 / sqrt(dimensionR), 6 / sqrt(dimensionR));
	}

    entityVecUpdate = (REAL *)calloc(entityTotal * dimension, sizeof(REAL));
    for (INT i = 0; i < entityTotal; i++) {
        for (INT ii=0; ii<dimension; ii++)
            entityVecUpdate[i * dimension + ii] = epsilon*epsilon;
    }

	for (INT i = 0; i < entityTotal; i++) {
		for (INT ii=0; ii < dimension; ii++)
			entityVec[i * dimension + ii] = randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));
		norm(entityVec + i * dimension, dimension);
	}

	for (INT i = 0; i < relationTotal; i++)
		for (INT j = 0; j < dimensionR; j++)
			for (INT k = 0; k < dimension; k++)
				matrix[i * dimension * dimensionR + j * dimension + k] =  randn(0, 1.0 / dimension, -6 / sqrt(dimension), 6 / sqrt(dimension));

	matrixUpdate = (REAL *)calloc(relationTotal * dimension * dimensionR, sizeof(REAL));
    for (INT i = 0; i < relationTotal; i++)
        for (INT j = 0; j < dimensionR; j++)
            for (INT k = 0; k < dimension; k++)
                matrixUpdate[i * dimension * dimensionR + j * dimension + k] = epsilon;

    fin = fopen((inPath + "train2id.txt").c_str(), "r");
	tmp = fscanf(fin, "%d", &tripleTotal);
	trainHead = (Triple *)calloc(tripleTotal * 3, sizeof(Triple));
	trainTail = trainHead + tripleTotal;
	trainList = trainTail + tripleTotal;
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
	sort(trainList, trainList + tripleTotal, cmp_list());

	lefHead = (INT *)calloc(entityTotal * 4, sizeof(INT));
	rigHead = lefHead + entityTotal;
	lefTail = rigHead + entityTotal;
	rigTail = lefTail + entityTotal;
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

	left_mean = (REAL *)calloc(relationTotal * 2,sizeof(REAL));
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

	if (initPath != "") {
		for (INT i = 0; i < relationTotal; i++)
			for (INT j = 0; j < dimensionR; j++)
				for (INT k = 0; k < dimension; k++)
					if (j == k)
						matrix[i * dimension * dimensionR + j * dimension + k] = 1;
					else
						matrix[i * dimension * dimensionR + j * dimension + k] = 0;
		FILE* f1 = fopen((initPath + "entity2vec" + note1 + ".vec").c_str(),"r");
		for (INT i = 0; i < entityTotal; i++) {
			for (INT ii = 0; ii < dimension; ii++)
				tmp = fscanf(f1, "%f", &entityVec[i * dimension + ii]);
			norm(entityVec + i * dimension, dimension);
		}
		fclose(f1);
		FILE* f2 = fopen((initPath + "relation2vec" + note1 + ".vec").c_str(),"r");
		for (INT i=0; i < relationTotal; i++) {
			for (INT ii=0; ii < dimension; ii++)
				tmp = fscanf(f2, "%f", &relationVec[i * dimensionR + ii]);
		}
		fclose(f2);
		printf("Inizializzato con TransE\n");
	}
	
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
    struct stat statbuf3;
    if (stat((loadPath + "A" + note + ".bin").c_str(), &statbuf3) != -1) {  
        INT fd = open((loadPath + "A" + note + ".bin").c_str(), O_RDONLY);
        REAL* matrixTmp =(REAL*)mmap(NULL, statbuf3.st_size, PROT_READ, MAP_PRIVATE, fd, 0); 
        memcpy(matrix, matrixTmp, relationTotal * dimensionR * dimension * sizeof(REAL));
        munmap(matrixTmp, statbuf3.st_size);
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
        INT last = i * dimensionR;
        for (INT j = 0; j < dimensionR; j++)
            tmp = fscanf(fin, "%f", &relationVec[last + j]);
    }
    fclose(fin);

    fin = fopen((loadPath + "A" + note + ".vec").c_str(), "r");
    for (INT i = 0; i < relationTotal; i++)
            for (INT jj = 0; jj < dimension; jj++)
                for (INT ii = 0; ii < dimensionR; ii++)
                    tmp = fscanf(fin, "%f", &matrix[i * dimensionR * dimension + jj + ii * dimension]);
    fclose(fin);
}

/*
	Training process of transR.
*/

INT transRLen;
INT transRBatch;
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


REAL calc_sum(INT e1, INT e2, INT rel, REAL *tmp1, REAL *tmp2) {
	INT lastM = rel * dimension * dimensionR;
	INT last1 = e1 * dimension;
	INT last2 = e2 * dimension;
	INT lastr = rel * dimensionR;
	INT lastrc = findcluster(rel) * dimensionR;
	REAL sum = 0;
	for (INT ii = 0; ii < dimensionR; ii++) {
		tmp1[ii] = tmp2[ii] = 0;
		for (INT jj = 0; jj < dimension; jj++) {
			tmp1[ii] += matrix[lastM + jj] * entityVec[last1 + jj];
			tmp2[ii] += matrix[lastM + jj] * entityVec[last2 + jj];
		}
		lastM += dimension;
		sum += fabs(tmp1[ii] + relationVec[lastr + ii] + relationCVec[lastrc + ii] - tmp2[ii]);
	}
	return sum;
}

void gradient(INT e1_a, INT e2_a, INT rel_a, INT belta, INT same, REAL *tmp1, REAL *tmp2) {
	INT lasta1 = e1_a * dimension;
	INT lasta2 = e2_a * dimension;
	INT lastar = rel_a * dimensionR;
    INT lastarc = findcluster(rel_a) * dimensionR;
	INT lastM = rel_a * dimensionR * dimension;
	REAL x;
	for (INT ii=0; ii < dimensionR; ii++) {
		x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii] - relationCVec[lastarc + ii];
		if (x > 0)
			x = belta * alpha;
		else
			x = -belta * alpha;
		for (INT jj = 0; jj < dimension; jj++) {
			matrixDao[lastM + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastM + jj]);
            if(matrixUpdate[lastM + jj]<limit and epoch>epoch_start)
			    matrixUpdate[lastM + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
			entityVecDao[lasta1 + jj] -= x * matrix[lastM + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
			    entityVecUpdate[lasta1 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
			entityVecDao[lasta2 + jj] += x * matrix[lastM + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
			    entityVecUpdate[lasta2 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
		}
		relationVecDao[lastar + ii] -= same * x*(1+lambdaR)/sqrt(relationVecUpdate[lastar + ii]);
        if(relationVecUpdate[lastar + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastar + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastarc + ii] -= same * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastarc + ii]);
        if(relationCVecUpdate[lastarc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastarc + ii] += (1+lambdaC)*(1+lambdaC);
		lastM = lastM + dimension;
	}
}

void gradientSubClass(INT cls1_a, INT cls2_a, INT cls1_b, INT cls2_b) {
    INT lasta1 = cls1_a * dimension;
    INT lasta2 = cls2_a * dimension;
    INT lastb1 = cls1_b * dimension;
    INT lastb2 = cls2_b * dimension;
    INT lastar = typeOf_id * dimensionR;
    INT lastM = typeOf_id * dimensionR * dimension;
    REAL* tmpp = (REAL *)calloc(dimensionR * 4, sizeof(REAL));
    REAL* tmpp1=tmpp;
    REAL* tmpp2=tmpp + dimensionR;
    REAL* tmpp3=tmpp + dimensionR*2;
    REAL* tmpp4=tmpp + dimensionR*3;
    REAL summ1 = calc_sum(cls1_a, cls2_a, typeOf_id, tmpp, tmpp + dimensionR);
    REAL summ2 = calc_sum(cls1_b, cls2_b, typeOf_id, tmpp + dimensionR * 2, tmpp + dimensionR * 3);
    REAL belta=-0.01;
    INT same=1;
    REAL x;

    //se entità equivalenti, salva array
    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmpp2[ii] -tmpp1[ii];
        //x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii];
        if (x > 0)
            x = belta * alpha;
        else
            x = -belta * alpha;
        for (INT jj = 0; jj < dimension; jj++) {
            matrixDao[lastM + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastM + jj]);
            if(matrixUpdate[lastM + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastM + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
            entityVecDao[lasta1 + jj] -= x * matrix[lastM + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
            entityVecDao[lasta2 + jj] += x * matrix[lastM + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
        }
        //relationVecDao[lastar + ii] -= same * x;
        lastM = lastM + dimension;
    }

    lastM = typeOf_id * dimensionR * dimension;
    belta=0.01;

    for (INT ii=0; ii < dimensionR; ii++) {
        //x = tmp4[ii] - tmp3[ii] - relationVec[lastar + ii];
        x = tmpp4[ii] - tmpp3[ii];
        if (x > 0)
            x = belta * alpha;
        else
            x = -belta * alpha;
        for (INT jj = 0; jj < dimension; jj++) {
            matrixDao[lastM + jj] -=  x * (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])/sqrt(matrixUpdate[lastM + jj]);
            if(matrixUpdate[lastM + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastM + jj]+= (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])*(entityVec[lastb1 + jj] - entityVec[lastb2 + jj]);
            entityVecDao[lastb1 + jj] -= x * matrix[lastM + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
            entityVecDao[lastb2 + jj] += x * matrix[lastM + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastM + jj]*matrix[lastM + jj];
        }
        //relationVecDao[lastar + ii] -= same * x;
        lastM = lastM + dimension;
    }

    norm(entityVecDao + dimension * cls1_a, dimension);
    norm(entityVecDao + dimension * cls2_a, dimension);
    norm(entityVecDao + dimension * cls1_b, dimension);
    norm(entityVecDao + dimension * cls2_b, dimension);
    norm(entityVecDao + dimension * cls1_a, matrixDao + dimension * dimensionR * typeOf_id);
    norm(entityVecDao + dimension * cls2_a, matrixDao + dimension * dimensionR * typeOf_id);
    norm(entityVecDao + dimension * cls1_b, matrixDao + dimension * dimensionR * typeOf_id);
    norm(entityVecDao + dimension * cls2_b, matrixDao + dimension * dimensionR * typeOf_id);

}

void gradientInverseOf(INT e1_a, INT e2_a, INT rel_a, INT belta, INT same, REAL *tmp1, REAL *tmp2,INT e1_b, INT e2_b, INT rel_b, INT belta2, INT same2, REAL *tmp3, REAL *tmp4, INT inverseRel) {
    INT lasta1 = e1_a * dimension;
    INT lasta2 = e2_a * dimension;
    INT lastar = rel_a * dimensionR;
    INT lastarc = findcluster(rel_a) * dimensionR;
    INT lastb1 = e1_b * dimension;
    INT lastb2 = e2_b * dimension;
    INT lastbr = rel_b * dimensionR;
    INT lastbrc = findcluster(rel_b) * dimensionR;
    INT lastMa = rel_a * dimensionR * dimension;
    INT lastMb = rel_b * dimensionR * dimension;
    INT lastMI = inverseRel * dimensionR * dimension;
    INT lastInverse = inverseRel * dimensionR;
    INT lastInverseC = findcluster(inverseRel) * dimensionR;

    REAL x,x2;
    //se entità equivalenti, salva array

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii] - relationCVec[lastarc + ii];
        x2 = tmp1[ii] - tmp2[ii] - relationVec[lastInverse + ii] - relationCVec[lastInverseC + ii];
        if (x > 0)
            x = belta * alpha;
        else
            x = -belta * alpha;
        if (x2 > 0)
            x2 = belta * alpha;
        else
            x2 = -belta * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMa + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastMa + jj]);
            if(matrixUpdate[lastMa + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMa + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
            entityVecDao[lasta1 + jj] -= x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];
            entityVecDao[lasta2 + jj] += x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];

            matrixDao[lastMI + jj] -=  x2 * (entityVec[lasta2 + jj] - entityVec[lasta1 + jj])/sqrt(matrixUpdate[lastMI + jj]);
            if(matrixUpdate[lastMI + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMI + jj]+= (entityVec[lasta2 + jj] - entityVec[lasta1 + jj])*(entityVec[lasta2 + jj] - entityVec[lasta1 + jj]);
            entityVecDao[lasta2 + jj] -= x2 * matrix[lastMI + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastMI + jj]*matrix[lastMI + jj];
            entityVecDao[lasta1 + jj] += x2 * matrix[lastMI + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastMI + jj]*matrix[lastMI + jj];

        }

        relationVecDao[lastar + ii] -= same * x*(1+lambdaR)/sqrt(relationVecUpdate[lastar + ii]);
        if(relationVecUpdate[lastar + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastar + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastarc + ii] -= same * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastarc + ii]);
        if(relationCVecUpdate[lastarc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastarc + ii] += (1+lambdaC)*(1+lambdaC);
        lastMa = lastMa + dimension;

        relationVecDao[lastInverse + ii] -= same * x2*(1+lambdaR)/sqrt(relationVecUpdate[lastInverse + ii]);
        if(relationVecUpdate[lastInverse + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastInverse + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastInverseC + ii] -= same * x2*(1+lambdaC)/sqrt(relationCVecUpdate[lastInverseC + ii]);
        if(relationCVecUpdate[lastInverseC + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastInverseC + ii] += (1+lambdaC)*(1+lambdaC);
        lastMI = lastMI + dimension;
    }

    lastMI = inverseRel * dimensionR * dimension;

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp4[ii] - tmp3[ii] - relationVec[lastbr + ii] - relationCVec[lastbrc + ii];
        x2 = tmp3[ii] - tmp4[ii] - relationVec[lastInverse + ii] - relationCVec[lastInverseC + ii];
        if (x > 0)
            x = belta2 * alpha;
        else
            x = -belta2 * alpha;
        if (x2 > 0)
            x2 = belta2 * alpha;
        else
            x2 = -belta2 * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMb + jj] -=  x * (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])/sqrt(matrixUpdate[lastMb + jj]);
            if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMb + jj]+= (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])*(entityVec[lastb1 + jj] - entityVec[lastb2 + jj]);
            entityVecDao[lastb1 + jj] -= x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
            entityVecDao[lastb2 + jj] += x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];

            matrixDao[lastMI + jj] -=  x2 * (entityVec[lastb2 + jj] - entityVec[lastb1 + jj])/sqrt(matrixUpdate[lastMI + jj]);
            if(matrixUpdate[lastMI + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMI + jj]+= (entityVec[lastb2 + jj] - entityVec[lastb1 + jj])*(entityVec[lastb2 + jj] - entityVec[lastb1 + jj]);
            entityVecDao[lastb2 + jj] -= x2 * matrix[lastMI + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastMI + jj]*matrix[lastMI + jj];
            entityVecDao[lastb1 + jj] += x2 * matrix[lastMI + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastMI + jj]*matrix[lastMI + jj];
        }

        relationVecDao[lastbr + ii] -= same2 * x*(1+lambdaR)/sqrt(relationVecUpdate[lastbr + ii]);
        if(relationVecUpdate[lastbr + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastbr + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastbrc + ii] -= same2 * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastbrc + ii]);
        if(relationCVecUpdate[lastbrc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastbrc + ii] += (1+lambdaC)*(1+lambdaC);

        lastMb = lastMb + dimension;

        relationVecDao[lastInverse + ii] -= same2 * x2*(1+lambdaR)/sqrt(relationVecUpdate[lastInverse + ii]);
        if(relationVecUpdate[lastInverse + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastInverse + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastInverseC + ii] -= same2 * x2*(1+lambdaC)/sqrt(relationCVecUpdate[lastInverseC + ii]);
        if(relationCVecUpdate[lastInverseC + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastInverseC + ii] += (1+lambdaC)*(1+lambdaC);

        lastMI = lastMI + dimension;
    }
    norm(relationVecDao + dimensionR * inverseRel, dimensionR);
    norm(relationCVec + dimensionR * findcluster(inverseRel), dimensionR);
    norm(entityVecDao + dimension * e1_a, dimension);
    norm(entityVecDao + dimension * e2_a, dimension);
    norm(entityVecDao + dimension * e1_b, dimension);
    norm(entityVecDao + dimension * e2_b, dimension);
    norm(entityVecDao + dimension * e1_a, matrixDao + dimension * dimensionR * inverseRel);
    norm(entityVecDao + dimension * e2_a, matrixDao + dimension * dimensionR * inverseRel);
    norm(entityVecDao + dimension * e1_b, matrixDao + dimension * dimensionR * inverseRel);
    norm(entityVecDao + dimension * e2_b, matrixDao + dimension * dimensionR * inverseRel);
}

void gradientEquivalentProperty(INT e1_a, INT e2_a, INT rel_a, INT belta, INT same, REAL *tmp1, REAL *tmp2,INT e1_b, INT e2_b, INT rel_b, INT belta2, INT same2, REAL *tmp3, REAL *tmp4, INT equivalent) {
    INT lasta1 = e1_a * dimension;
    INT lasta2 = e2_a * dimension;
    INT lastar = rel_a * dimension;
    INT lastarc = findcluster(rel_a) * dimensionR;
    INT lastb1 = e1_b * dimension;
    INT lastb2 = e2_b * dimension;
    INT lastbr = rel_b * dimension;
    INT lastbrc = findcluster(rel_b) * dimensionR;
    INT lastMa = rel_a * dimensionR * dimension;
    INT lastMb = rel_b * dimensionR * dimension;
    INT lastME = equivalent * dimensionR * dimension;
    INT lastEquivalent = equivalent * dimension;
    INT lastEquivalentC = findcluster(equivalent) * dimensionR;

    REAL x,x2;

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii] - relationCVec[lastarc +ii];
        x2 = tmp2[ii] - tmp1[ii] - relationVec[lastEquivalent + ii] - relationCVec[lastEquivalentC +ii];
        if (x > 0)
            x = belta * alpha;
        else
            x = -belta * alpha;
        if (x2 > 0)
            x2 = belta * alpha;
        else
            x2 = -belta * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMa + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastMa + jj]);
            if(matrixUpdate[lastMa + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMa + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
            entityVecDao[lasta1 + jj] -= x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];
            entityVecDao[lasta2 + jj] += x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];

            matrixDao[lastMa + jj] -=  x2 * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastMa + jj]);
            if(matrixUpdate[lastMa + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMa + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
            entityVecDao[lasta1 + jj] -= x2 * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];
            entityVecDao[lasta2 + jj] += x2 * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];

        }


        relationVecDao[lastar + ii] -= same * x*(1+lambdaR)/sqrt(relationVecUpdate[lastar + ii]);
        if(relationVecUpdate[lastar + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastar + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastarc + ii] -= same * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastarc + ii]);
        if(relationCVecUpdate[lastarc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastarc + ii] += (1+lambdaC)*(1+lambdaC);


        relationVecDao[lastEquivalent + ii] -= same * x2*(1+lambdaR)/sqrt(relationVecUpdate[lastEquivalent + ii]);
        if(relationVecUpdate[lastEquivalent + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastEquivalent + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastEquivalentC + ii] -= same * x2*(1+lambdaC)/sqrt(relationCVecUpdate[lastEquivalentC + ii]);
        if(relationCVecUpdate[lastEquivalentC + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastEquivalentC + ii] += (1+lambdaC)*(1+lambdaC);


        lastMa = lastMa + dimension;
    }

    lastEquivalent = equivalent * dimension;

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp4[ii] - tmp3[ii] - relationVec[lastbr + ii] - relationCVec[lastbrc +ii];
        x2 = tmp4[ii] - tmp3[ii] - relationVec[lastEquivalent + ii] - relationCVec[lastEquivalentC +ii];
        if (x > 0)
            x = belta2 * alpha;
        else
            x = -belta2 * alpha;
        if (x2 > 0)
            x2 = belta2 * alpha;
        else
            x2 = -belta2 * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMb + jj] -=  x * (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])/sqrt(matrixUpdate[lastMb + jj]);
            if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMb + jj]+= (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])*(entityVec[lastb1 + jj] - entityVec[lastb2 + jj]);
            entityVecDao[lastb1 + jj] -= x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
            entityVecDao[lastb2 + jj] += x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];

            matrixDao[lastMb + jj] -=  x2 * (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])/sqrt(matrixUpdate[lastMb + jj]);
            if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMb + jj]+= (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])*(entityVec[lastb1 + jj] - entityVec[lastb2 + jj]);
            entityVecDao[lastb1 + jj] -= x2 * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
            entityVecDao[lastb2 + jj] += x2 * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
        }

        relationVecDao[lastbr + ii] -= same2 * x*(1+lambdaR)/sqrt(relationVecUpdate[lastbr + ii]);
        if(relationVecUpdate[lastbr + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastbr + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastbrc + ii] -= same2 * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastbrc + ii]);
        if(relationCVecUpdate[lastbrc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastbrc + ii] += (1+lambdaC)*(1+lambdaC);

        relationVecDao[lastEquivalent + ii] -= same2 * x2*(1+lambdaR)/sqrt(relationVecUpdate[lastEquivalent + ii]);
        if(relationVecUpdate[lastEquivalent + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastEquivalent + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastEquivalentC + ii] -= same2 * x2*(1+lambdaC)/sqrt(relationCVecUpdate[lastEquivalentC + ii]);
        if(relationCVecUpdate[lastEquivalentC + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastEquivalentC + ii] += (1+lambdaC)*(1+lambdaC);

        lastMb = lastMb + dimension;
    }

    norm(relationVecDao + dimensionR * equivalent, dimensionR);
    norm(relationCVec + dimensionR * findcluster(equivalent), dimensionR);
    norm(entityVecDao + dimension * e1_a, dimension);
    norm(entityVecDao + dimension * e2_a, dimension);
    norm(entityVecDao + dimension * e1_b, dimension);
    norm(entityVecDao + dimension * e2_b, dimension);
    norm(entityVecDao + dimension * e1_a, matrixDao + dimension * dimensionR * equivalent);
    norm(entityVecDao + dimension * e2_a, matrixDao + dimension * dimensionR * equivalent);
    norm(entityVecDao + dimension * e1_b, matrixDao + dimension * dimensionR * equivalent);
    norm(entityVecDao + dimension * e2_b, matrixDao + dimension * dimensionR * equivalent);
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

void gradientEquivalentClass(INT e1_a, INT e2_a, INT rel_a, INT belta, INT same, REAL *tmp1, REAL *tmp2,INT e1_b, INT e2_b, INT rel_b, INT belta2, INT same2, REAL *tmp3, REAL *tmp4, INT id) {
    INT lasta1 = e1_a * dimension;
    INT lasta2 = e2_a * dimension;
    INT lastar = rel_a * dimensionR;
    INT lastarc = findcluster(rel_a) * dimensionR;
    INT lastb1 = e1_b * dimension;
    INT lastb2 = e2_b * dimension;
    INT lastbr = rel_b * dimensionR;
    INT lastbrc = findcluster(rel_b) * dimensionR;
    INT lastMa = rel_a * dimensionR * dimension;
    INT lastMb = rel_b * dimensionR * dimension;
    REAL x;
    vector<int> corr_tails;
    vector<int> corr_heads;
    set<int> eq_cls = getEquivalentClass(e2_a);

    int j=0;
    for(set<int>::iterator it = eq_cls.begin(); it != eq_cls.end(); ++it) {
        int tail = getFalseClass(e1_a);
        if(tail == -1)
            tail = corrupt_head(id, e1_a, rel_a);
        int head = corrupt_tail(id, (*it), rel_a);
        corr_tails.push_back(tail);
        corr_heads.push_back(head);
        j++;
    }

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp2[ii] - tmp1[ii] - relationVec[lastar + ii] - relationCVec[lastarc +ii];
        if (x > 0)
            x = belta * alpha;
        else
            x = -belta * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMa + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])/sqrt(matrixUpdate[lastMa + jj]);
            if(matrixUpdate[lastMa + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMa + jj]+= (entityVec[lasta1 + jj] - entityVec[lasta2 + jj])*(entityVec[lasta1 + jj] - entityVec[lasta2 + jj]);
            entityVecDao[lasta1 + jj] -= x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
            if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta1 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];
            entityVecDao[lasta2 + jj] += x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta2 + jj]);
            if(entityVecUpdate[lasta2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lasta2 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];

        }
        relationVecDao[lastar + ii] -= same * x*(1+lambdaR)/sqrt(relationVecUpdate[lastar + ii]);
        if(relationVecUpdate[lastar + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastar + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastarc + ii] -= same * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastarc + ii]);
        if(relationCVecUpdate[lastarc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastarc + ii] += (1+lambdaC)*(1+lambdaC);
        lastMa = lastMa + dimension;
    }

    lastMa = rel_a * dimensionR * dimension;

    for (INT ii=0; ii < dimensionR; ii++) {
        x = tmp4[ii] - tmp3[ii] - relationVec[lastbr + ii] - relationCVec[lastbrc +ii];
        if (x > 0)
            x = belta2 * alpha;
        else
            x = -belta2 * alpha;
        for (INT jj = 0; jj < dimension; jj++) {

            matrixDao[lastMb + jj] -=  x * (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])/sqrt(matrixUpdate[lastMb + jj]);
            if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                matrixUpdate[lastMb + jj]+= (entityVec[lastb1 + jj] - entityVec[lastb2 + jj])*(entityVec[lastb1 + jj] - entityVec[lastb2 + jj]);
            entityVecDao[lastb1 + jj] -= x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb1 + jj]);
            if(entityVecUpdate[lastb1 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb1 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
            entityVecDao[lastb2 + jj] += x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastb2 + jj]);
            if(entityVecUpdate[lastb2 + jj]<limit and epoch>epoch_start)
                entityVecUpdate[lastb2 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];

        }

        relationVecDao[lastbr + ii] -= same2 * x*(1+lambdaR)/sqrt(relationVecUpdate[lastbr + ii]);
        if(relationVecUpdate[lastbr + ii]<limit and epoch>epoch_start)
            relationVecUpdate[lastbr + ii]+= (1+lambdaR)*(1+lambdaR);
        relationCVec[lastbrc + ii] -= same2 * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastbrc + ii]);
        if(relationCVecUpdate[lastbrc + ii]<limit and epoch>epoch_start)
            relationCVecUpdate[lastbrc + ii] += (1+lambdaC)*(1+lambdaC);

        lastMb = lastMb + dimension;
    }

    lastMb = rel_b * dimensionR * dimension;

    if(eq_cls.size() > 0) {
        int i = 0;
        for(set<int>::iterator it = eq_cls.begin(); it != eq_cls.end(); ++it) {
            INT lastCls = (*it) * dimension;
            REAL* tmpp = (REAL *)calloc(dimensionR * 4, sizeof(REAL));
            REAL* tmpp1=tmpp;
            REAL* tmpp2=tmpp + dimensionR;
            REAL* tmpp3=tmpp + dimensionR*2;
            REAL* tmpp4=tmpp + dimensionR*3;
            INT flag=0;
            REAL summ1 = calc_sum(e1_a, (*it), rel_a, tmpp, tmpp + dimensionR);

            for (INT ii=0; ii < dimensionR; ii++) {
                x = tmpp2[ii] - tmpp1[ii] - relationVec[lastar + ii]- relationCVec[lastarc +ii];
                if (x > 0)
                    x = belta * alpha *0.1;
                else
                    x = -belta * alpha*0.1;
                for (INT jj = 0; jj < dimension; jj++) {

                    matrixDao[lastMa + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[lastCls + jj])/sqrt(matrixUpdate[lastMa + jj]);
                    if(matrixUpdate[lastMa + jj]<limit and epoch>epoch_start)
                        matrixUpdate[lastMa + jj]+= (entityVec[lasta1 + jj] - entityVec[lastCls + jj])*(entityVec[lasta1 + jj] - entityVec[lastCls + jj]);
                    entityVecDao[lasta1 + jj] -= x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
                    if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                        entityVecUpdate[lasta1 + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];
                    entityVecDao[lastCls + jj] += x * matrix[lastMa + jj]/sqrt(entityVecUpdate[lastCls + jj]);
                    if(entityVecUpdate[lastCls + jj]<limit and epoch>epoch_start)
                        entityVecUpdate[lastCls + jj]+= matrix[lastMa + jj]*matrix[lastMa + jj];

                }
                relationVecDao[lastar + ii] -= same * x*(1+lambdaR)/sqrt(relationVecUpdate[lastar + ii]);
                if(relationVecUpdate[lastar + ii]<limit and epoch>epoch_start)
                    relationVecUpdate[lastar + ii]+= (1+lambdaR)*(1+lambdaR);
                relationCVec[lastarc + ii] -= same * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastarc + ii]);
                if(relationCVecUpdate[lastarc + ii]<limit and epoch>epoch_start)
                    relationCVecUpdate[lastarc + ii] += (1+lambdaC)*(1+lambdaC);
                lastMa = lastMa + dimension;
            }

            lastMa = rel_a * dimensionR * dimension;

            uint pr;
            if (bernFlag)
                pr = 1000 * right_mean[rel_a] / (right_mean[rel_a] + left_mean[rel_a]);
            else
                pr = 500;
            flag=0;
            if (randd(id) % 1000 < pr) {
                int tail = corr_tails[i];
                INT last_tail = tail * dimension;
                REAL summ2 = calc_sum(e1_a, tail, rel_b, tmpp + dimensionR * 2, tmpp + dimensionR * 3);
                for (INT ii=0; ii < dimensionR; ii++) {
                    x = tmpp4[ii] - tmpp3[ii] - relationVec[lastbr + ii]- relationCVec[lastbrc +ii];
                    if (x > 0)
                        x = belta2 * alpha*0.1;
                    else
                        x = -belta2 * alpha*0.1;
                    for (INT jj = 0; jj < dimension; jj++) {

                        matrixDao[lastMb + jj] -=  x * (entityVec[lasta1 + jj] - entityVec[last_tail + jj])/sqrt(matrixUpdate[lastMb + jj]);
                        if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                            matrixUpdate[lastMb + jj]+= (entityVec[lasta1 + jj] - entityVec[last_tail + jj])*(entityVec[lasta1 + jj] - entityVec[last_tail + jj]);
                        entityVecDao[lasta1 + jj] -= x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lasta1 + jj]);
                        if(entityVecUpdate[lasta1 + jj]<limit and epoch>epoch_start)
                            entityVecUpdate[lasta1 + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
                        entityVecDao[last_tail + jj] += x * matrix[lastMb + jj]/sqrt(entityVecUpdate[last_tail + jj]);
                        if(entityVecUpdate[last_tail + jj]<limit and epoch>epoch_start)
                            entityVecUpdate[last_tail + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
                    }
                    relationVecDao[lastbr + ii] -= same2 * x*(1+lambdaR)/sqrt(relationVecUpdate[lastbr + ii]);
                    if(relationVecUpdate[lastbr + ii]<limit and epoch>epoch_start)
                        relationVecUpdate[lastbr + ii]+= (1+lambdaR)*(1+lambdaR);
                    relationCVec[lastbrc + ii] -= same2 * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastbrc + ii]);
                    if(relationCVecUpdate[lastbrc + ii]<limit and epoch>epoch_start)
                        relationCVecUpdate[lastbrc + ii] += (1+lambdaC)*(1+lambdaC);
                    lastMb = lastMb + dimension;
                }
                flag=1;
           } else {
                int head = corr_heads[i];
                INT last_head = head * dimension;
                REAL summ2 = calc_sum(head, (*it), rel_b, tmpp + dimensionR * 2, tmpp + dimensionR * 3);
                for (INT ii=0; ii < dimensionR; ii++) {
                    x = tmpp4[ii] - tmpp3[ii] - relationVec[lastbr + ii]- relationCVec[lastbrc +ii];
                    if (x > 0)
                        x = belta2 * alpha*0.1;
                    else
                        x = -belta2 * alpha*0.1;
                    for (INT jj = 0; jj < dimension; jj++) {

                        matrixDao[lastMb + jj] -=  x * (entityVec[last_head + jj] - entityVec[lastCls + jj])/sqrt(matrixUpdate[lastMb + jj]);
                        if(matrixUpdate[lastMb + jj]<limit and epoch>epoch_start)
                            matrixUpdate[lastMb + jj]+= (entityVec[last_head + jj] - entityVec[lastCls + jj])*(entityVec[last_head + jj] - entityVec[lastCls + jj]);
                        entityVecDao[last_head + jj] -= x * matrix[lastMb + jj]/sqrt(entityVecUpdate[last_head + jj]);
                        if(entityVecUpdate[last_head + jj]<limit and epoch>epoch_start)
                            entityVecUpdate[last_head + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];
                        entityVecDao[lastCls + jj] += x * matrix[lastMb + jj]/sqrt(entityVecUpdate[lastCls + jj]);
                        if(entityVecUpdate[lastCls + jj]<limit and epoch>epoch_start)
                            entityVecUpdate[lastCls + jj]+= matrix[lastMb + jj]*matrix[lastMb + jj];

                    }
                    relationVecDao[lastbr + ii] -= same2 * x*(1+lambdaR)/sqrt(relationVecUpdate[lastbr + ii]);
                    if(relationVecUpdate[lastbr + ii]<limit and epoch>epoch_start)
                        relationVecUpdate[lastbr + ii]+= (1+lambdaR)*(1+lambdaR);
                    relationCVec[lastbrc + ii] -= same2 * x*(1+lambdaC)/sqrt(relationCVecUpdate[lastbrc + ii]);
                    if(relationCVecUpdate[lastbrc + ii]<limit and epoch>epoch_start)
                        relationCVecUpdate[lastbrc + ii] += (1+lambdaC)*(1+lambdaC);

                    lastMb = lastMb + dimension;
                }
                flag=2;
            }
        lastMb = rel_b * dimensionR * dimension;
            if(flag==1){
                norm(relationVecDao + dimensionR * rel_a, dimensionR);
                norm(relationCVec + dimensionR * findcluster(rel_a), dimensionR);
                norm(entityVecDao + dimension * e1_a, dimension);
                norm(entityVecDao + dimension * (*it), dimension);
                norm(entityVecDao + dimension * corr_tails[i], dimension);
                norm(entityVecDao + dimension * e1_a, matrixDao + dimension * dimensionR * rel_a);
                norm(entityVecDao + dimension * (*it), matrixDao + dimension * dimensionR * rel_a);
                norm(entityVecDao + dimension * corr_tails[i], matrixDao + dimension * dimensionR * rel_a);
            } else{
                if(flag==2){
                    norm(relationVecDao + dimensionR * rel_a, dimensionR);
                    norm(relationCVec + dimensionR * findcluster(rel_a), dimensionR);
                    norm(entityVecDao + dimension * e1_a, dimension);
                    norm(entityVecDao + dimension * (*it), dimension);
                    norm(entityVecDao + dimension * corr_heads[i], dimension);
                    norm(entityVecDao + dimension * e1_a, matrixDao + dimension * dimensionR * rel_a);
                    norm(entityVecDao + dimension * (*it), matrixDao + dimension * dimensionR * rel_a);
                    norm(entityVecDao + dimension * corr_heads[i], matrixDao + dimension * dimensionR * rel_a);
                }
            }
            i++;
        }
    }

}


void train_kb(INT e1_a, INT e2_a, INT rel_a, INT e1_b, INT e2_b, INT rel_b, REAL *tmp,INT id) {
	REAL sum1 = calc_sum(e1_a, e2_a, rel_a, tmp, tmp + dimensionR);
	REAL sum2 = calc_sum(e1_b, e2_b, rel_b, tmp + dimensionR * 2, tmp + dimensionR * 3);
	if (sum1 + margin > sum2) {
		res += margin + sum1 - sum2;
        if(rel_a == typeOf_id) {
            gradientEquivalentClass(e1_a, e2_a, rel_a, -1, 1, tmp, tmp + dimensionR,e1_b, e2_b, rel_b, 1, 1, tmp + dimensionR * 2, tmp + dimensionR * 3,id);
        } else if(getInverse(rel_a) != -1) {
            gradientInverseOf(e1_a, e2_a, rel_a, -1, 1, tmp, tmp + dimensionR,e1_b, e2_b, rel_b, 1, 1, tmp + dimensionR * 2, tmp + dimensionR * 3, getInverse(rel_a));
        } else if(getEquivalentProperty(rel_a) != -1) {
            gradientEquivalentProperty(e1_a, e2_a, rel_a, -1, 1, tmp, tmp + dimensionR,e1_b, e2_b, rel_b, 1, 1, tmp + dimensionR * 2, tmp + dimensionR * 3, getEquivalentProperty(rel_a));
        } else {
            gradient(e1_a, e2_a, rel_a, -1, 1, tmp, tmp + dimensionR);
            gradient(e1_b, e2_b, rel_b, 1, 1, tmp + dimensionR * 2, tmp + dimensionR * 3);
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
int train(int triple_index, int id,int range, int domain,REAL* tmp) {
    int j = -1;
    uint pr;
    if (bernFlag)
        pr = 1000*right_mean[trainList[triple_index].r]/(right_mean[trainList[triple_index].r]+left_mean[trainList[triple_index].r]);
    else
        pr = 500;
    if (randd(id) % 1000 < pr || isFunctional(trainList[triple_index].r)) {
        if(hasRange(trainList[triple_index].r)) {
            j = getTailCorrupted(triple_index, id);
        }
        if(j == -1)
            j = corrupt_head(id, trainList[triple_index].h, trainList[triple_index].r);
        train_kb(trainList[triple_index].h, trainList[triple_index].t, trainList[triple_index].r, trainList[triple_index].h, j, trainList[triple_index].r, tmp,id);
    } else {
        if(hasDomain(trainList[triple_index].r))
            j = getHeadCorrupted(triple_index, id);
        if(j == -1)
            j = corrupt_tail(id, trainList[triple_index].t, trainList[triple_index].r);
        train_kb(trainList[triple_index].h, trainList[triple_index].t, trainList[triple_index].r, j, trainList[triple_index].t, trainList[triple_index].r, tmp,id);
    }
    tot_batches++;
    return j;
}


void* trainMode(void *con) {
	INT id, i, j, pr;
	id = (unsigned long long)(con);
	next_random[id] = rand();
	REAL *tmp = tmpValue + id * dimensionR * 4;
	for (INT k = transRBatch / threads; k >= 0; k--) {
		i = rand_max(id, transRLen);
        //otteni il dominio della relazione e la classe della entità testa che dovrà rispettarlo
        int domain = -1;
        if(rel2domain.find(trainList[i].r) != rel2domain.end()){
            domain = rel2domain.find(trainList[i].r)->second;
        }
        //otteni il codominio (range) della relazione e la classe della entità coda che dovrà rispettarlo
        int range = -1;
        if(rel2range.find(trainList[i].r) != rel2range.end()){
            range = rel2range.find(trainList[i].r)->second;
        }
		j = train(i,id,range,domain,tmp);
		norm(trainList[i].h, trainList[i].t, trainList[i].r, j);
        norm(relationCVec + dimensionR * findcluster(trainList[i].r), dimensionR);

	}
	pthread_exit(NULL);
}

void train(void *con) {
    srand(time(0));
	transRLen = tripleTotal;
	transRBatch = transRLen / nbatches;
	next_random = (unsigned long long *)calloc(threads, sizeof(unsigned long long));
	tmpValue = (REAL *)calloc(threads * dimensionR * 4, sizeof(REAL));
	memcpy(relationVecDao, relationVec, dimensionR * relationTotal * sizeof(REAL));
	memcpy(entityVecDao, entityVec, dimension * entityTotal * sizeof(REAL));
	memcpy(matrixDao, matrix, dimension * relationTotal * dimensionR * sizeof(REAL));
	for (epoch = 0; epoch < trainTimes; epoch++) {
		res = 0;
        for (INT batch = 0; batch < nbatches; batch++) {
			pthread_t *pt = (pthread_t *)malloc(threads * sizeof(pthread_t));
			for (long a = 0; a < threads; a++)
				pthread_create(&pt[a], NULL, trainMode,  (void*)a);
			for (long a = 0; a < threads; a++)
				pthread_join(pt[a], NULL);
			free(pt);
			memcpy(relationVec, relationVecDao, dimensionR * relationTotal * sizeof(REAL));
			memcpy(entityVec, entityVecDao, dimension * entityTotal * sizeof(REAL));
			memcpy(matrix, matrixDao, dimension * relationTotal * dimensionR * sizeof(REAL));
		}
		printf("epoch %d %f\n", epoch, res);
	}
}

/*
	Get the results of transR.
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
		FILE* f1 = fopen((outPath + "A" + note + ".bin").c_str(), "wb");
		len = relationTotal * dimension * dimensionR; tot = 0;
		head = matrix;
		while (tot < len) {
			INT sum = fwrite(head + tot, sizeof(REAL), len - tot, f1);
			tot = tot + sum;
		}
		fclose(f1);
}

void out() {
		if (outBinaryFlag) {
			out_binary(); 
			return;
		}
		FILE* f2 = fopen((outPath + "relation2vec" + note + ".vec").c_str(), "w");
		FILE* f3 = fopen((outPath + "entity2vec" + note + ".vec").c_str(), "w");
		for (INT i = 0; i < relationTotal; i++) {
			INT last = dimension * i;
            INT lastc= dimensionR * findcluster(i);
			for (INT ii = 0; ii < dimension; ii++)
				fprintf(f2, "%.6f\t", (relationVec[last + ii]+relationCVec[lastc+ii]));
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
		FILE* f1 = fopen((outPath + "A" + note + ".vec").c_str(),"w");
		for (INT i = 0; i < relationTotal; i++)
			for (INT jj = 0; jj < dimension; jj++) {
				for (INT ii = 0; ii < dimensionR; ii++)
					fprintf(f1, "%f\t", matrix[i * dimensionR * dimension + jj + ii * dimension]);
				fprintf(f1,"\n");
			}
		fclose(f1);
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
	if ((i = ArgPos((char *)"-sizeR", argc, argv)) > 0) dimensionR = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-input", argc, argv)) > 0) inPath = argv[i + 1];
	if ((i = ArgPos((char *)"-output", argc, argv)) > 0) outPath = argv[i + 1];
	if ((i = ArgPos((char *)"-init", argc, argv)) > 0) initPath = argv[i + 1];
	if ((i = ArgPos((char *)"-load", argc, argv)) > 0) loadPath = argv[i + 1];
	if ((i = ArgPos((char *)"-thread", argc, argv)) > 0) threads = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-epochs", argc, argv)) > 0) trainTimes = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-nbatches", argc, argv)) > 0) nbatches = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-alpha", argc, argv)) > 0) alpha = atof(argv[i + 1]);
	if ((i = ArgPos((char *)"-margin", argc, argv)) > 0) margin = atof(argv[i + 1]);
	if ((i = ArgPos((char *)"-load-binary", argc, argv)) > 0) loadBinaryFlag = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-out-binary", argc, argv)) > 0) outBinaryFlag = atoi(argv[i + 1]);
	if ((i = ArgPos((char *)"-note", argc, argv)) > 0) note = argv[i + 1];
	if ((i = ArgPos((char *)"-note1", argc, argv)) > 0) note1 = argv[i + 1];
}

int main(int argc, char **argv) {
	setparameters(argc, argv);
    readingclusters();
    printf("Prepare\n");
	init();
    owlInit();
    if (loadPath != "") load();
    printf("Train\n");
    train(NULL);
	if (outPath != "") out();
	return 0;
}
