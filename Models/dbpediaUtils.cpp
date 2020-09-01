#include <fstream>
#include <string>
#include <iostream>
#include <map>
#include <list>
#include <vector>
#include <set>
using namespace std;

const string path = "NELL_small/";
const string type_of = "<http://www.w3.org/1999/02/22-rdf-syntax-ns#type>";
map<string, int> class2id; //dbpedia class -> ID
map<string, int> rel2id; //dbpedia class -> ID
multimap<int,int> sub2superclass; //mappa id in id della superclasse

struct TypeOf {
	int e, c; // ID_Entity -> ID_Class (Of entity2IDFile)
};

bool batched(int id, list<int> id_list) {
    for (list<int>::iterator it=id_list.begin(); it != id_list.end(); ++it)
        if(id == *it) {
        	return true;
        }
    return false;
}

void dbpedia2id() {
	//creating map structure for dbpedia classes from class2id.txt
	string tmp;
	ifstream class_file(path + "OWLderived/class2id.txt");
	while (getline(class_file, tmp)) {
		string::size_type pos=tmp.find(' ',0);
		string dbp_class= tmp.substr(0,pos);
		int class_id = atoi(tmp.substr(pos+1).c_str());
		class2id.insert(pair<string,int>(dbp_class, class_id));
	}
	class_file.close();
}

void relation2id() {
	//creating map structure for relation relation2id.txt
    ifstream relation_file(path + "relation2id.txt");
    string tmp;
    while (getline(relation_file, tmp)) {
        string::size_type pos=tmp.find('\t',0);
    	string rel= tmp.substr(0,pos);
    	int temp_id = atoi(tmp.substr(pos+1).c_str());
    	rel2id.insert(pair<string,int>(rel,temp_id));
    }
    relation_file.close();
}

void subclassMap() {
	ifstream subclass_file(path + "OWLderived/subclassof.txt");
	string tmp;
	while(getline(subclass_file,tmp)) {
		string::size_type pos=tmp.find(' ',0);
		string subclass= tmp.substr(0,pos);
		string superclass = tmp.substr(pos+1);
		int id_subclass = class2id.find(subclass)->second;
		int id_superclass = class2id.find(superclass)->second;
		sub2superclass.insert(pair<int,int>(id_subclass,id_superclass));
	}
}

void getentity2dbpedia_class() {
    // GET ID Of the relation rdf:type
    int type_id = -1;
    map<string,int>::iterator x = rel2id.find(type_of);
    if(x != rel2id.end()) {
    	type_id = x->second;
    	cout << type_of << " id: " << type_id << endl;
    } else {
    	cout << "Failed to find relation: " + type_of << endl;
    	return;
    }

    string tmp;

    //get triples with relation's id = type_id
    vector<TypeOf> triple_vec;  //entity -> class id
    list<int> entity_id_class; //unique id class

    ifstream triple_file(path + "triple2id.txt");
    while (getline(triple_file, tmp)) {
        string::size_type pos=tmp.find(' ',0);
    	string e1= tmp.substr(0,pos);
    	string e2_rel = tmp.substr(pos+1);
    	pos = e2_rel.find(' ', 0);
    	string e2 = e2_rel.substr(0,pos);
    	string rel = e2_rel.substr(pos+1);
    	if(atoi(rel.c_str()) == type_id) {
    		TypeOf t;
            t.e = atoi(e1.c_str());
            t.c = atoi(e2.c_str());
            triple_vec.push_back(t);
            int x =atoi(e2.c_str());
            if(!batched(x, entity_id_class))
                entity_id_class.push_back(x);
        }
    }
    triple_file.close();

    //get dbpedia id class using entity2ID.txt
    map<int,int> id2dbp_id; //id_entity -> dbpedia class
    ifstream entity_file(path + "entity2id.txt");
    ofstream todbpedia(path +"todbpedia.txt");
    getline(entity_file, tmp);
    int num_ent = atoi(tmp.c_str());
    int counter_finish=1;
    while (getline(entity_file, tmp)) {
        cout << counter_finish<< " : " << num_ent << endl;
        counter_finish++;
        string::size_type pos=tmp.find('\t',0);
    	string maybe_class= tmp.substr(0,pos);
        int entity_id = atoi(tmp.substr(pos+1).c_str());
        //verifico se è una classe e se ha una classe corrispondente in owl
    	if(batched(entity_id, entity_id_class) && class2id.end() != class2id.find(maybe_class)) {
            //salvo l'associazione class in entity a class in dbpedia
			int id_dbpedia_class = class2id.find(maybe_class)->second;//ottengo id dbpedia da class2id usando entity
			id2dbp_id.insert(pair<int,int>(entity_id, id_dbpedia_class));
			todbpedia << entity_id << "\t" <<  id_dbpedia_class << endl;
        }
    }
    entity_file.close();
    todbpedia.close();

//    //Per ogni coppia entità-classe scrivo su file entità-dbpedia_id
//    string output_folder = path + "entity2class.txt";
//    ofstream output(output_folder);
//    for(uint i = 0; i < triple_vec.size(); i++) {
//    	if(id2dbp_id.end() != id2dbp_id.find(triple_vec[i].c))
//    		output << triple_vec[i].e << "\t" << id2dbp_id.find(triple_vec[i].c)->second << endl;
//    }
//    cout << "Printed on " << output_folder << endl;
//    output.close();

    multimap<int,int> entity2Class;
    for(uint i = 0; i < triple_vec.size(); i++) {
		if(id2dbp_id.find(triple_vec[i].c) != id2dbp_id.end()) {
			int id_dbpedia = id2dbp_id.find(triple_vec[i].c)->second;
			string out;
			set<int,greater <int>> tempClasses; //set da riempire con tutte le superclassi incontrate
			tempClasses.insert(id_dbpedia);
			pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
			set<int>::iterator it;
			//per ogni sottoclasse incontrata
			while(!tempClasses.empty()) {
				it = tempClasses.begin();
				id_dbpedia =  *it;
                cout << "entity: " << triple_vec[i].e << "id_dbpedia: " << id_dbpedia << endl;
				//aggiungo una associazione entità->classe
				entity2Class.insert(pair<int,int>(triple_vec[i].e,id_dbpedia));
				//cancello la classe in uso
				tempClasses.erase(it);
				ret = sub2superclass.equal_range(id_dbpedia); //restituisce tutti i valori con chiave=id_dbpedia
				//esploro tutte le superclassi e le aggiungo all'insieme
				for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it) {
					tempClasses.insert(it->second);
					//termino quando trovo la radice
				}
			}
		}
	}

    //print entity -> class
    //multiple entry, caricare in un multimap per usare
	ofstream output(path +"entity2class.txt");
	for (int k = 0; k < num_ent; ++k) {
    	pair <std::multimap<int,int>::iterator, multimap<int,int>::iterator> ret;
		ret = entity2Class.equal_range(k);
		set<int> classes;
		for (multimap<int,int>::iterator it=ret.first; it!=ret.second; ++it)
			classes.insert(it->second);

		if(!classes.empty()) {
			for(set<int>::iterator t = classes.begin(); t != classes.end(); ++t)
				output << k << "\t" << *t << endl;
		}
	}
	cout << "Scritte classi e superclassi in: entity2class.txt";
}

/*	 1 leggi elenco proprietà da relation2id in un map str->id
 *	 2 Per ogni proprietà-range-dominio
 *			Se proprietà esiste in relation2id
 *				conserva id proprietà
 *				converti dominio in id (usa map class2id)
 *				scrivi su file id_rel id_domain
 *	Domain definisce la classe della testa di una tripla
*/
void domain2id() {
	//passo 1 chiamando relation2id
	ifstream domain_file(path + "OWLderived/rs_domain.txt");
	ofstream out_domain(path + "rs_domain2id.txt");
	string tmp;
	while(getline(domain_file, tmp)) {
		string::size_type pos=tmp.find('\t',0);
		string rel= tmp.substr(0,pos);
		string domain = tmp.substr(pos+1);
		if(rel2id.find(rel) != rel2id.end()) {
			int id_rel = rel2id.find(rel)->second;
			int id_dom = class2id.find(domain)->second;
			out_domain << id_rel << " " << id_dom << endl;
		}
	}
}

/*	 1 leggi elenco proprietà da relation2id in un map str->id
 *	 2 Per ogni proprietà-range-dominio
 *			Se proprietà esiste in relation2id
 *				conserva id proprietà
 *				converti dominio in id (usa map class2id)
 *				scrivi su file id_rel id_domain
 *	 Range definisce la classe della coda di una tripla
*/
void range2id() {
	//passo 1 chiamando relation2id
	ifstream range_file(path + "OWLderived/rs_range.txt");
	ofstream out_range(path + "rs_range2id.txt");
	string tmp;
	while(getline(range_file, tmp)) {
		string::size_type pos=tmp.find('\t',0);
		string rel= tmp.substr(0,pos);
		string range = tmp.substr(pos+1);
		if(rel2id.find(rel) != rel2id.end()) {
			int id_rel = rel2id.find(rel)->second;
			int id_dom = class2id.find(range)->second;
			out_range << id_rel << " " << id_dom << endl;
		}
	}
}

void subclass2file() {
	ifstream subclass_file(path + "OWLderived/subclassof.txt");
	ofstream output(path +"subclassID.txt");
	string tmp;
	while(getline(subclass_file,tmp)) {
		string::size_type pos=tmp.find(' ',0);
		string subclass= tmp.substr(0,pos);
		string superclass = tmp.substr(pos+1);
		int id_subclass = class2id.find(subclass)->second;
		int id_superclass = class2id.find(superclass)->second;
		output << id_subclass << "\t" << id_superclass << endl;
	}
	cout << "printed on " << "/subclassof.txt" << endl;
}

void prepare() {
	dbpedia2id();
	relation2id();
	subclassMap();
}



int main()
{
	prepare();

	getentity2dbpedia_class();
    range2id();
    domain2id();
	subclass2file();
    return 0;
}
