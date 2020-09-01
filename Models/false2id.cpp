//============================================================================
// Name        : linkedDataUtils.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <fstream>
#include <string>
#include <list>
#include <iostream>
#include <map>
#include <vector>
#include <set>

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

string datapath = "NELL_small/";

const REAL pi = 3.141592653589793238462643383;

int relationTotal,entityTotal,tripleTotal;
int *freqRel, *freqEnt;
float *left_mean, *right_mean;
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
map<string, int> entities,relations;
const string typeOf = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
int typeOf_id;

struct Triple{
    int h,r,t;
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
    INT res = (rand() *rand()) % x;
    while (res<=0) {
        res += x;
    }
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
void getSubclass() {
    multimap<int,int> sub2superclass; //mappa id in id della superclasse
    ifstream subclass_file(datapath + "subclassID.txt");
    string tmp;
    while(getline(subclass_file,tmp)) {
        string::size_type pos=tmp.find('\t',0);
        int id_subclass= atoi(tmp.substr(0,pos).c_str());
        int id_superclass = atoi(tmp.substr(pos+1).c_str());
        sub2superclass.insert(pair<int,int>(id_subclass,id_superclass));
    }
    subclass_file.close();

    ifstream todbp_file(datapath + "todbpedia.txt");
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
    cout << "number or Subclass:" << entsubclass.size() << endl;
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


/*
bool batched(string item, list<string> item_list) {
    for (list<string>::iterator it=item_list.begin(); it != item_list.end(); ++it)
        if(item.compare(*it) == 0) {
        	return true;
        }
    return false;
}

void tsvTotriple() {
	std::locale::global(std::locale(""));
	ifstream tsv_file(datapath);
	ofstream output(datapath + "triples.txt");
	string str;
    while (getline(tsv_file, str)) {
    	string::size_type pos;
    	pos=str.find('\t',0);
    	string last_part= str.substr(pos+1);
    	string head = str.substr(1,pos-2);

    	pos=last_part.find('\t',0);
		string relation = last_part.substr(1,pos-2);

		string tail= last_part.substr(pos+2);
		tail = tail.substr(0, tail.size()-1);
		output << "<" << head << "> <" << relation << "> <" << tail << ">"<<endl;
    }
    cout << "printed on " << datapath + "triples.txt" << endl;
}
*/

void count(){
    ifstream file(datapath + "triples.txt");
    string str;
    int cnt_triple = 0;
    while (getline(file, str)) {
    	cnt_triple++;
    	if(cnt_triple%100)
    		cout << cnt_triple << endl;
    }
    file.close();
}

void resource2ID()
{
    set<string> entities, relations;
    ifstream file(datapath + "triples.txt");
    ofstream out_ent(datapath + "entity2id.txt");
    ofstream out_rel(datapath + "relation2id.txt");

    string str;
    int cnt_triple = 0;
    int id_ent = 0, id_rel = 0;

    while (getline(file, str))
    {
    	cout << cnt_triple++ << endl;
    	string::size_type pos;
    	pos=str.find(' ',0);
    	string last_part= str.substr(pos+1);
    	string head = str.substr(0,pos);
    	entities.insert(head);

    	pos=last_part.find(' ',0);
    	string relation = last_part.substr(0,pos);

    	relations.insert(relation);

    	string tail= last_part.substr(pos+1);
    	//tail = tail.substr(0, tail.size()-1);
        tail = tail.substr(0, tail.size());
		entities.insert(tail);
		cout << head << relation << tail <<endl;
	}
    //print on file
    out_ent << entities.size() << endl;
    for(set<string>::iterator it = entities.begin(); it != entities.end(); it++) {
    	//out_ent << "<" << (*it) << ">" << "\t" << id_ent++ <<  endl;
        out_ent << (*it) << "\t" << id_ent++ <<  endl;
    }
    out_rel << relations.size() << endl;
    for(set<string>::iterator it = relations.begin(); it != relations.end(); it++) {
    	//out_rel << "<" << (*it) << ">" << "\t" << id_rel++ <<  endl;
        out_rel << (*it) << "\t" << id_rel++ <<  endl;
    }
    file.close();
    out_ent.close();
    out_rel.close();

    cout << "Entities: " << entities.size() << endl;
    cout << "Relations: " << relations.size() << endl;

}

void triple2ID() {

    map<string, int> entities,relations;

    ifstream triples_file(datapath + "triples.txt");
    ifstream false_file(datapath + "falseTypeOf.txt");
    ifstream ent_file(datapath + "entity2id.txt");
    ifstream rel_file(datapath + "relation2id.txt");
    ofstream out_triple(datapath + "triple2id.txt");
    ofstream false_out_file(datapath + "falseTypeOf2id.txt");

    string str;
    getline(ent_file, str); //number of entities
    while (getline(ent_file, str)) {
    	string::size_type pos;
		pos=str.find('\t',0);
		string entity = str.substr(0,pos);
		int id_ent = atoi(str.substr(pos+1).c_str());
		entities.insert(pair<string,int>(entity,id_ent));
    }
    ent_file.close();

    getline(ent_file, str); //number of relations
    while (getline(rel_file, str)) {
    	string::size_type pos;
		pos=str.find('\t',0);
		string relation = str.substr(0,pos);
		int id_rel = atoi(str.substr(pos+1).c_str());
		relations.insert(pair<string,int>(relation,id_rel));
    }
    rel_file.close();


    set<string> train_ids;
    int cnt_triple = 0;
    while (getline(triples_file, str)) {
		cout << cnt_triple<< endl;
		string::size_type pos = str.find(' ',0);
		string last_part= str.substr(pos+1);
		//string head = "<" + str.substr(0,pos) + ">";
        string head = str.substr(0,pos);

		pos=last_part.find(' ',0);
		//string relation = "<" + last_part.substr(0,pos) + ">";
        string relation = last_part.substr(0,pos) ;

		string tail= last_part.substr(pos+1);
		//tail = "<" + tail.substr(0, tail.size()-1) + ">";
        tail = tail.substr(0, tail.size());

        //cout << head << "\t" << relation << "\t" << tail;

		if(entities.find(head)!=entities.end() && entities.find(tail)!=entities.end() && relations.find(relation)!=relations.end()) {
			int id_head = entities.find(head)->second;
			int id_rel = relations.find(relation)->second;
			int id_tail = entities.find(tail)->second;
			string triple = to_string(id_head) + " " + to_string(id_tail) + " " + to_string(id_rel);
			train_ids.insert(triple);
			cnt_triple++;
		}
	}
    out_triple << train_ids.size() << endl;
    for(set<string>::iterator it = train_ids.begin(); it != train_ids.end(); it++) {
    	out_triple << (*it) << "\n";
    }
    triples_file.close();
    out_triple.close();

    set<string> false_ids;
    cnt_triple = 0;
    while (getline(false_file, str)) {
        cout << cnt_triple<< endl;
        string::size_type pos = str.find(' ',0);
        string last_part= str.substr(pos+1);
        //string head = "<" + str.substr(0,pos) + ">";
        string head = str.substr(0,pos);

        pos=last_part.find(' ',0);
        //string relation = "<" + last_part.substr(0,pos) + ">";
        string tail = last_part.substr(0,pos) ;

        string relation= last_part.substr(pos+1);
        //tail = "<" + tail.substr(0, tail.size()-1) + ">";
        relation = relation.substr(0, relation.size());

        cout << head << relation << tail;

        if(entities.find(head)!=entities.end() && entities.find(tail)!=entities.end() && relations.find(relation)!=relations.end()) {
            int id_head = entities.find(head)->second;
            int id_rel = relations.find(relation)->second;
            int id_tail = entities.find(tail)->second;
            string triple = to_string(id_head) + " " + to_string(id_tail) + " " + to_string(id_rel);
            false_ids.insert(triple);
            cnt_triple++;
        }
    }
    false_out_file << false_ids.size() << endl;
    for(set<string>::iterator it = false_ids.begin(); it != false_ids.end(); it++) {
        false_out_file << (*it) << "\n";
    }
    false_file.close();
    false_out_file.close();
}

void sampleDataset() {
    ifstream file_triple(datapath + "triple2id.txt");
    ofstream train2id(datapath + "train2id.txt");
    ofstream test2id(datapath + "test2id.txt");
    ofstream valid(datapath + "valid2id.txt");

    vector<string> triples;

    string str;
	getline(file_triple, str); //number of entities
	while (getline(file_triple, str))
		triples.push_back(str);

	srand(time(0));
	int n = triples.size();
	for(int i = 0; i < n-2; i++) {
		int j = rand() % (n-i) + i;
		string temp = triples[i];
		triples[i] = triples[j];
		triples[j] = temp;
	}

//	for(int i = 0; i < triples.size(); i++)
//		cout << triples[i] << endl;

	int valid_pos = n * 10 / 100;
	int test_pos = n * 20 / 100;

	cout << valid_pos << endl;
	cout << test_pos << endl;
	for(int i = 0; i < valid_pos; i++)
		valid << triples[i] << endl;
	for(int j = valid_pos; j < test_pos; j++)
		test2id << triples[j] << endl;
	for(int k = test_pos; k < n; k++)
		train2id << triples[k] << endl;

	valid.close();
	test2id.close();
	train2id.close();

}

int corrupt_head(int id, int h, int r) {
    int lef, rig, mid, ll, rr;
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
    int tmp = rand(id, entityTotal - (rr - ll + 1));
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

int corrupt_tail(int id, int t, int r) {
    int lef, rig, mid, ll, rr;
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
    int tmp = rand(id, entityTotal - (rr - ll + 1));
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

void false2idfile(){

    ofstream false_file(datapath + "false2id.txt");

    string str,tmp;
    vector<Triple> false2id;


    ifstream domain_file(datapath + "rs_domain2id.txt");
    while (getline(domain_file, tmp)) {
        string::size_type pos=tmp.find(' ',0);
        int relation= atoi(tmp.substr(0,pos).c_str());
        int domain = atoi(tmp.substr(pos+1).c_str());
        rel2domain.insert(pair<int,int>(relation,domain));
    }
    domain_file.close();

    ifstream range_file(datapath + "rs_range2id.txt");
    while (getline(range_file, tmp)) {
        string::size_type pos=tmp.find(' ',0);
        int relation= atoi(tmp.substr(0,pos).c_str());
        int range = atoi(tmp.substr(pos+1).c_str());
        rel2range.insert(pair<int,int>(relation,range));
    }
    range_file.close();

    map<string,int> rel2id;
    map<string,int> ent2id;

    printf("Loading ontology information...\n");

    //carico le relazioni per il confronto
    ifstream rel_file(datapath + "relation2id.txt");
    getline(rel_file,tmp);
    while (getline(rel_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
        string rel = tmp.substr(0,pos);
        int id = atoi(tmp.substr(pos+1).c_str());
        rel2id.insert(pair<string,int>(rel,id));
        if(rel == typeOf)
            typeOf_id = id;
    }
    rel_file.close();

    //carico le entità per il confronto
    ifstream ent_file(datapath + "entity2id.txt");
    getline(ent_file,tmp);
    while (getline(ent_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
        string ent = tmp.substr(0,pos);
        int id = atoi(tmp.substr(pos+1).c_str());
        ent2id.insert(pair<string,int>(ent,id));
    }
    ent_file.close();

    ifstream class_file(datapath + "entity2class.txt");
    while (getline(class_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
        int entity= atoi(tmp.substr(0,pos).c_str());
        int class_id = atoi(tmp.substr(pos+1).c_str());
        ent2class.insert(pair<int,int>(entity,class_id));
    }
    class_file.close();


    srand(time(0));
    //next_random = (unsigned long long *)calloc(1, sizeof(unsigned long long));
    for(int i=0;i<tripleTotal;i++){
        INT id, pr, j;
        Triple tr;
        id = rand();
        //next_random[id] = rand();
        pr = 500;
        //cout << id <<endl;
        if (rand() % 1000 < pr) {
            if(hasRange(trainList[i].r)) {
                j = getTailCorrupted(i, id);
            }
            if(j == -1) {
                j = corrupt_head(id, trainList[i].h, trainList[i].r);

            }
           tr.h=trainList[i].h;
           tr.r=trainList[i].r;
            j = corrupt_head(id, trainList[i].h, trainList[i].r);
           if(j>=entityTotal){
               j=j%entityTotal;
           }
           if(j<0){
               j=j*(-1);
               j=j%(entityTotal);
           }
           tr.t=j;
           false2id.push_back(tr);

        } else {
            if(hasDomain(trainList[i].r)) {
                j = getHeadCorrupted(i, id);
            }
            if(j == -1) {
                j = corrupt_tail(id, trainList[i].t, trainList[i].r);
            }
            j = corrupt_tail(id, trainList[i].t, trainList[i].r);
            if(j>=entityTotal){
                j=j%entityTotal;
            }
            if(j<0){
                j=j*(-1);
                j=j%(entityTotal);
            }
            tr.h=j;
            tr.r=trainList[i].r;
            tr.t=trainList[i].t;
            false2id.push_back(tr);
        }
    }

    for(int i=0;i<false2id.size();i++){
        if(false2id[i].r!=typeOf_id) {
            false_file << false2id[i].h << " " << false2id[i].t << " " << false2id[i].r << endl;
        }
    }
    false_file.close();
}


void init() {

    FILE *fin;
    int tmp;


    fin = fopen((datapath + "relation2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%d", &relationTotal);
    fclose(fin);


    fin = fopen((datapath + "entity2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%d", &entityTotal);
    fclose(fin);



    freqRel = (int *)calloc(relationTotal + entityTotal, sizeof(int));
    freqEnt = freqRel + relationTotal;

    fin = fopen((datapath + "test2id.txt").c_str(), "r");
    tmp = fscanf(fin, "%d", &tripleTotal);
    trainHead = (Triple *)calloc(tripleTotal, sizeof(Triple));
    trainTail = (Triple *)calloc(tripleTotal, sizeof(Triple));
    trainList = (Triple *)calloc(tripleTotal, sizeof(Triple));
    for (int i = 0; i < tripleTotal; i++) {
        tmp = fscanf(fin, "%d", &trainList[i].h);
        tmp = fscanf(fin, "%d", &trainList[i].t);
        tmp = fscanf(fin, "%d", &trainList[i].r);
        freqEnt[trainList[i].t]++;
        freqEnt[trainList[i].h]++;
        freqRel[trainList[i].r]++;
        trainHead[i] = trainList[i];
        trainTail[i] = trainList[i];
    }
    fclose(fin);

    sort(trainHead, trainHead + tripleTotal, cmp_head());
    sort(trainTail, trainTail + tripleTotal, cmp_tail());

    lefHead = (int *)calloc(entityTotal, sizeof(INT));
    rigHead = (int *)calloc(entityTotal, sizeof(INT));
    lefTail = (int *)calloc(entityTotal, sizeof(INT));
    rigTail = (int *)calloc(entityTotal, sizeof(INT));
    memset(rigHead, -1, sizeof(INT)*entityTotal);
    memset(rigTail, -1, sizeof(INT)*entityTotal);
    for (int i = 1; i < tripleTotal; i++) {
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

    left_mean = (float *)calloc(relationTotal * 2, sizeof(REAL));
    right_mean = left_mean + relationTotal;
    for (int i = 0; i < entityTotal; i++) {
        for (int j = lefHead[i] + 1; j <= rigHead[i]; j++)
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

    for (int i = 0; i < relationTotal; i++) {
        left_mean[i] = freqRel[i] / left_mean[i];
        right_mean[i] = freqRel[i] / right_mean[i];
    }

}

int main() {
	//resource2ID();
	//triple2ID();
	//sampleDataset();

	init();
	getSubclass();
	false2idfile();
	return 0;
}
