import copy
import os
import sys
import cv2
import numpy as np
#import pandas as pd
import matplotlib
import matplotlib.pyplot as plt

matplotlib.use("pgf")
matplotlib.rcParams.update({
    "pgf.texsystem": "pdflatex",
    'font.family': 'serif',
    'text.usetex': True,
    'pgf.rcfonts': False,
})

if __name__ == "__main__":
    '''
    xTransE=[1.5,5]
    xTransOWL=[2,5.5]
    xTransR=[2.5,6]
    xTransROWL=[3,6.5]
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[8004,7542],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[8036,7585],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[8116,7588],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[7748,7249],hatch='-',label="TransROWL-HRS AdaGrad 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[10035,9678],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[11886,11516],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[9206,8822],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[9471,9091],hatch='-',label="TransROWL 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[587,157],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[580,162],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[656,187],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[617,171],hatch='-',label="TransROWL 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[2872,2709],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[2263,2093],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[2315,2140],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[2334,2161],hatch='-',label="TransROWL 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[28,39.6],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[27.9,39.6],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[26.6,35],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[29.5,41],hatch='-',label="TransROWL-HRS AdaGrad 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[39,47.6],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[46.4,56.6],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[45.7,59.1],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[46.2,58.78],hatch='-',label="TransROWL-HRS AdaGrad 1000epoch")
    '''
    '''
    plt.bar(xTransE,width=0.5, height=[9.75,15.7],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[13.2,20.85],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[85,95.5],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[85.12,96.85],hatch='-',label="TransROWL-HRS Momentum 1000epoch")
    
    plt.bar(xTransE,width=0.5, height=[8.71,19.42],hatch='|',label="TransE 1000epoch")
    plt.bar(xTransOWL,width=0.5, height=[8.68,19.43],hatch='/',label="TransOWL 1000epoch")
    plt.bar(xTransR,width=0.5, height=[82,89],hatch='+',label="TransR 1000epoch")
    plt.bar(xTransROWL,width=0.5, height=[84.85,95.2],hatch='-',label="TransROWL-HRS AdaGrad 1000epoch")
    '''

    #y=[2.5,6]
    #plt.xticks(y, ['RAW','FILTERED'])
    #plt.ylim(7000, 8500)
    #plt.ylim(8000, 13000)
    #plt.ylim(0, 800)
    #plt.ylim(1700, 3200)
    #plt.ylim(20, 55)
    #plt.ylim(35, 73)
    #plt.ylim(5, 135)
    
    
    

    '''
    plt.scatter( 0.389,0.993, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.3586,0.9006, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter( 0.3368,0.9983, s=40, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.3608,0.9949, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter(0.00388,0.389, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.05808,0.3586, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.000816,0.3368, s=40, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.00285,0.3608, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter( 0.9425,0.666, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.8346,0.908, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter( 0.9097,0.8526, s=40, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.768,0.9652, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter(0.8913,0.9425, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.337,0.8346, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.6353,0.9097, s=40, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.106,0.768, s=40, marker='x',color='red',label="TransROWL 500epoch")

    plt.scatter(0.428,0.914, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.441,0.887, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.3245,0.964, s=60, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.3275,0.9635, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter(0.0065,0.428, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.091,0.441, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.0174,0.3245, s=60, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.018,0.3275, s=40, marker='x',color='red',label="TransROWL 500epoch")

    plt.scatter(0.8406,0.969, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.6878,0.9606, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.3345,0.955, s=60, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.35066,0.988, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter(0.1435,0.8406, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.0827,0.6878, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.023,0.3345, s=60, marker='+',color='green',label="TransR 500epoch")
    plt.scatter(0.006325,0.35066, s=40, marker='x',color='red',label="TransROWL 500epoch")
    
    plt.scatter(0.691,0.755, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.6707,0.677, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.6355,0.843, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.628,0.8471, s=40, marker='x',color='red',label="TransROWL-HRS Momentum 1000epoch")
    
    plt.scatter(0.4203,0.691, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.4928,0.6707, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.245,0.6355, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.2837,0.628, s=40, marker='x',color='red',label="TransROWL-HRS Momentum 1000epoch")
    
    plt.scatter(0.9002,0.2763, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.6152,0.4297, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.5193,0.3885, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.4372,0.4214, s=40, marker='x',color='red',label="TransROWL-HRS Momentum 1000epoch")
    
    plt.scatter(0.959,0.9002, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.6797,0.6152, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.6296,0.5193, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.516,0.4372, s=40, marker='x',color='red',label="TransROWL-HRS Momentum 1000epoch")
    
    plt.scatter(0.4068,0.9914, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.407,0.967, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.36417,0.9977, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.3474,0.99714, s=40, marker='x',color='red',label="TransROWL 1000epoch")
    
    plt.scatter(0.0058,0.4068, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.0228,0.407, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.00128,0.36417, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.0015,0.3474, s=40, marker='x',color='red',label="TransROWL 1000epoch")
    
    plt.scatter(0.9581,0.7809, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.93287,0.98963, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.9460,0.96644, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.882,0.999, s=40, marker='x',color='red',label="TransROWL 1000epoch")
    '''    

    plt.scatter(0.8651,0.9581, s=40, marker='o',color='blue',label="TransE 1000epoch")
    plt.scatter(0.1270,0.93287, s=40, marker='v',color='orange',label="TransOWL 1000epoch")
    plt.scatter(0.3783,0.9460, s=60, marker='+',color='green',label="TransR 1000epoch")
    plt.scatter(0.006,0.882, s=40, marker='x',color='red',label="TransROWL 1000epoch")
        

    #plt.ylim(0.87, 1)
    #plt.xlim(0.2,0.6)
    #plt.ylim(0.2, 0.5)
    #plt.xlim(-0.01,0.07)
    #plt.ylim(0.5, 1)
    #plt.xlim(0.7,1.1)
    #plt.ylim(0.4, 1.4)
    #plt.xlim(0,1)
    #plt.ylim(0.8, 1)
    #plt.xlim(0.25,0.5)
    #plt.ylim(0.2, 0.6)
    #plt.xlim(-0.005,0.11)
    #plt.ylim(0.8, 1.1)
    #plt.xlim(-0.1,1)
    #plt.ylim(0.2, 1.1)
    #plt.xlim(-0.05,0.25)
    #plt.ylim(0.6, 1)
    #plt.xlim(0.5,0.8)
    #plt.ylim(0.5, 0.8)
    #plt.xlim(0.2,0.55)
    #plt.ylim(0.2, 0.6)
    #plt.xlim(0.3,1)
    #plt.ylim(0.25, 1)
    #plt.xlim(0.4,1.2)
    #plt.ylim(0.90, 1)
    #plt.xlim(0.1,0.5)
    #plt.ylim(0.1, 0.5)
    #plt.xlim(0.0001,0.03)
    #plt.ylim(0.7, 1.1)
    #plt.xlim(0.8,1)

    plt.ylim(0.7, 1.1)
    plt.xlim(0,0.9)

    #print(x)
    #plt.hist(x,bins=[2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21],color='#0504aa',alpha=0.7, rwidth=0.85)
    #plt.hist(x,bins=[2,3,4,5,6,7,8,9,10,11,12,13,14,15],color='#0504aa',alpha=0.7, rwidth=0.85)
    
    #plt.xlabel("Recall",fontsize=15)
    #plt.ylabel("Precision",fontsize=15)
    plt.xlabel("FP Rate",fontsize=15)
    plt.ylabel("Recall",fontsize=15)
    plt.legend(loc="upper right",prop={'size': 7})
    plt.title('ROC Space',fontsize=18)
    #plt.title('Precision-Recall Space',fontsize=18)
    plt.show()
    '''
    # Generate data on commute times.
    size, scale = 1000, 10
    commutes = pd.Series(np.random.gamma(scale, size=size) ** 1.5)
    
    commutes.plot.hist(grid=True, bins=x, rwidth=0.9,color='#607c8e')
    plt.title('Commute Times for 1,000 Commuters')
    plt.xlabel('Counts')
    plt.ylabel('Commute Time')
    plt.grid(axis='y', alpha=0.75)
    plt.show()
    '''
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_MeanRank_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_MeanRank_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_MeanRank_TO_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_MeanRank_TO_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_Hits_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_Hits_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_Hits_TO_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'LP_Hits_TO_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_TO_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_TO_NEG.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_TO_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_TO_POS.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_NELL.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_NELL.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_TO_NELL.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_TO_NELL.pgf')

    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_15K.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_15K.pgf')
    #plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'PR_TO_15K.pgf')
    plt.savefig('/home/mihaelkeehl/Documenti/Tesi/MaterialeCodice/Creazioni/TransROWL - HRS/Grafici/'+'ROC_TO_15K.pgf')