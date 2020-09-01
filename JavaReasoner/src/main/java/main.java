import java.io.FileNotFoundException;
import java.io.UnsupportedEncodingException;

public class main {

    public static void main (String[] args) throws FileNotFoundException, UnsupportedEncodingException {
        reasoner r = new reasoner(args[0],args[1],args[2]);
        switch(args[3]) {
            case "equivalentClass":
                r.generateEquivalentClass();
                break;
            case "equivalentProperty":
                r.generateEquivalentPropertyClass();
                break;
            case "functionalProperty":
                r.generateFunctionalPropertyClass();
                break;
            case "inverseOf":
                r.generateInverseOfClass();
                break;
            case "subClassOf":
                r.generateSubClassOf();
                break;
            case "rangeDomain":
                r.rangeDomain();
                r.getRangeDomainFromSer();
                break;
            case "DisjointWith":
                r.generateDisjointClass();
                r.corruptClass();
                break;
            case "classList":
                r.class2id();
                break;
            case "merge":
                r.yagoMerge(args[4],args[5],args[6],args[7],args[8]);
                break;
        }
    }
}
