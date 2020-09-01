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

using namespace std;

string datapath = "NELL_small/";

bool batched(string item, list<string> item_list) {
    for (list<string>::iterator it=item_list.begin(); it != item_list.end(); ++it)
        if(item.compare(*it) == 0) {
        	return true;
        }
    return false;
}

void tsvTotriple() {
	std::locale::global(std::locale(""));
	ifstream tsv_file("/home/jhonny/Scaricati/my_dataset");
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
    	tail = tail.substr(0, tail.size()-1);
		entities.insert(tail);
	}
    //print on file
    out_ent << entities.size() << endl;
    for(set<string>::iterator it = entities.begin(); it != entities.end(); it++) {
    	out_ent << (*it) << "\t" << id_ent++ <<  endl;
    }
    out_rel << relations.size() << endl;
    for(set<string>::iterator it = relations.begin(); it != relations.end(); it++) {
    	out_rel <<  (*it) << "\t" << id_rel++ <<  endl;
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
        string relation = last_part.substr(0,pos);

		string tail= last_part.substr(pos+1);
		//tail = "<" + tail.substr(0, tail.size()-1) + ">";
        tail = tail.substr(0, tail.size()-1) ;

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
        relation =  relation.substr(0, relation.size()) ;

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

}
int main() {
	resource2ID();
	triple2ID();
	sampleDataset();
	return 0;
}
