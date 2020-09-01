import numpy as np
import math
import random
import matplotlib.pyplot as plt
#import seaborn as sb
#import pandas as pd
import random

owl_relation=[]
owl_entity=[]
fast_relation=[]
fast_entity=[]
dimension=100
dimensionR=100
relationTotal=0

files_src = "/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/NELL_small/"

def printA(A):
    with open("/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Test_acc/" +"TEST_A.txt", "a") as myfile:
        for i in range(0,relationTotal):
            for jj in range(0,dimension):
                for ii in range(0,dimensionR):
                    val=fast_A[i*dimensionR*dimension + jj + ii*dimension]
                    myfile.write(str(val)+"\t")
                myfile.write("\n")

def score(h,t,r):
    lastM = r * dimension * dimensionR
    ssum=0
    tmp1=np.zeros(dimensionR)
    tmp2=np.zeros(dimensionR)
    for ii in range(0,dimensionR):
        tmp1[ii]=0
        tmp2[ii]=0
        for jj in range(0,dimension):
            tmp1[ii]=tmp1[ii] + (fast_A[lastM + jj]*fast_entity[h][jj])
            tmp2[ii]=tmp2[ii] + (fast_A[lastM + jj]*fast_entity[t][jj])
        lastM=lastM + dimension
        ssum=ssum + abs(tmp1[ii] + fast_relation[r][ii] - tmp2[ii])
    fast_res = ssum
    owl_res = (owl_entity[h] + owl_relation[r] - owl_entity[t])
    return (-fast_res),(-np.linalg.norm(owl_res,1))

file_in = open(files_src + 'entity2vec_E.vec', 'r')
for line in file_in:
    x=line.split()
    sub_mat = []
    for val in x: 
        sub_mat.append(float(val))
    owl_entity.append(sub_mat)
owl_entity = np.array(owl_entity)

file_in = open(files_src + 'relation2vec_E.vec', 'r')
for line in file_in:
    x=line.split()
    sub_mat = []
    for val in x: 
        sub_mat.append(float(val))
    owl_relation.append(sub_mat)
owl_relation = np.array(owl_relation)


file_in = open(files_src + 'entity2vec_1000epoch.vec', 'r')
for line in file_in:
    x=line.split()
    sub_mat = []
    for val in x: 
        sub_mat.append(float(val))
    fast_entity.append(sub_mat)
fast_entity = np.array(fast_entity)

file_in = open(files_src + 'relation2vec_1000epoch.vec', 'r')
for line in file_in:
    x=line.split()
    sub_mat = []
    for val in x: 
        sub_mat.append(float(val))
    fast_relation.append(sub_mat)
fast_relation = np.array(fast_relation)


k=0
relationTotal=len(fast_relation)
print(relationTotal)
fast_A=np.zeros(relationTotal * dimension *dimensionR)
file_in = open(files_src + 'A_1000epoch.vec', 'r')
sub_mat = []
for line in file_in:
    x=line.split()
    for val in x:
        sub_mat.append(float(val))
    
for i in range(0,relationTotal):
    for jj in range(0,dimension):
        for ii in range(0,dimensionR):
            fast_A[i * dimensionR * dimension + jj + ii * dimension]=sub_mat[k]
            k=k+1

#ritrovare il valore delta per ogni relazione r
#printA(fast_A)

train=[]
file_in = open(files_src + 'train2id.txt', 'r')
first_line = True
for line in file_in:
    if(first_line):
        first_line = False
    else:
        x=line.split()
        triple=[]
        for val in x:
            triple.append(int(val))
        train.append(triple)
train = np.array(train)

acc_f = []
prec_f = []
rec_f = []
fpr_f = []

acc_o = []
prec_o = []
rec_o = []
fpr_o = []

for i in range(1):
    random.shuffle(train)
    num_rel = len(owl_relation)
    owl_delta_r = []
    fast_delta_r = []
    fail = 0
    for i in range(0,num_rel):
        delta_min = -0.75
        sum_fast = []
        sum_owl = []
        num = 0
        limit = 40
        for val in train:
            if(num > limit):
                break;
            if(i == val[2]):
                num += 1
                fast_temp, owl_temp = score(val[0], val[1], val[2])
                sum_owl.append(owl_temp)
                sum_fast.append(fast_temp)
        if(num > 0):
            owl_delta_r.append(float(np.min(sum_owl)))
            fast_delta_r.append(float(np.min(sum_fast)))
        else:
            fail += 1
            owl_delta_r.append(float(delta_min))
            fast_delta_r.append(float(delta_min))
    
    #test
    test_file = files_src + 'test2id.txt'
    fast_result=[]
    owl_result=[]
    file_in = open(test_file, 'r')
    first_line = True   #salto prima riga, contiene numero di triple
    num_test = 0;
    for line in file_in:
        if(first_line):
            x = line.split()
            first_line = False
            for val in x:
                num_test = int(val)
        else:
            x=line.split()
            triple=[]
            for val in x:
                triple.append(int(val))
            if(triple[2] == 0):
                fast_res, owl_res = score(triple[0], triple[1], triple[2])
                if(fast_res >= fast_delta_r[triple[2]]):
                    fast_result.append(1)
                else:
                    fast_result.append(0)
                if(owl_res >= owl_delta_r[triple[2]]):
                    owl_result.append(1)
                else:
                    owl_result.append(0)
    
    #test
    test_file = files_src + 'falseTypeOf2id.txt'
    f_fast_result=[]
    f_owl_result=[]
    file_in = open(test_file, 'r')
    first_line = True   #salto prima riga, contiene numero di triple
    for line in file_in:
        if(random.random() < 0.4):
            if(first_line):
                first_line = False
            else:
                x=line.split()
                triple=[]
                for val in x:
                    triple.append(int(val))
                fast_res, owl_res = score(triple[0], triple[1], triple[2])
                if(fast_res >= fast_delta_r[triple[2]]):
                    f_fast_result.append(0)
                else:
                    f_fast_result.append(1)
                if(owl_res >= owl_delta_r[triple[2]]):
                    f_owl_result.append(0)
                else:
                    f_owl_result.append(1)
    
    acc_f.append((fast_result.count(1)+f_fast_result.count(1))/ (len(fast_result) + len(f_fast_result)))
    prec_f.append((fast_result.count(1)/(fast_result.count(1)+f_fast_result.count(0))))
    rec_f.append(fast_result.count(1)/len(fast_result))
    fpr_f.append(f_fast_result.count(0)/(f_fast_result.count(0)+fast_result.count(0)))
    
    acc_o.append((owl_result.count(1)+f_owl_result.count(1))/ (len(owl_result) + len(f_owl_result)))
    prec_o.append(owl_result.count(1)/(owl_result.count(1)+f_owl_result.count(0)))
    rec_o.append(owl_result.count(1)/len(owl_result))
    fpr_o.append(f_owl_result.count(0)/(f_owl_result.count(0)+owl_result.count(0)))

with open("/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Test_acc/" +"result_to_1000epoch.txt", "a") as myfile:
    myfile.write("isa_acc_FAST.append(" + str(np.mean(acc_f)) + ")\n")
    myfile.write("isa_prec_FAST.append(" + str(np.mean(prec_f)) + ")\n")
    myfile.write("isa_rec_FAST.append(" + str(np.mean(rec_f)) + ")\n")
    myfile.write("isa_fpr_FAST.append(" + str(np.mean(fpr_f)) + ")\n")
    
    myfile.write("isa_acc_OWL.append(" + str(np.mean(acc_o)) + ")\n")
    myfile.write("isa_prec_OWL.append(" + str(np.mean(prec_o)) + ")\n")
    myfile.write("isa_rec_OWL.append(" + str(np.mean(rec_o)) + ")\n")
    myfile.write("isa_fpr_OWL.append(" + str(np.mean(fpr_o)) + ")\n")

print("Finish")
# print("--TransE--")
# print("Accuracy:",np.mean(acc_f))
# print("Precision:",np.mean(prec_f))
# print("Recall:", np.mean(rec_f))
# print("FPR:",np.mean(fpr_f))
# 
# print("--TransOWL--")
# print("Accuracy: ",np.mean(acc_o))
# print("Precision:",np.mean(prec_o))
# print("Recall:",np.mean(rec_o))
# print("FPR:",np.mean(fpr_o))
