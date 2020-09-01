import org.apache.jena.base.Sys;
import org.apache.jena.ext.com.google.common.collect.ArrayListMultimap;
import org.apache.jena.ext.com.google.common.collect.Multimap;
import org.apache.jena.rdf.model.*;
import org.apache.jena.reasoner.Derivation;
import org.apache.jena.reasoner.Reasoner;
import org.apache.jena.reasoner.ReasonerRegistry;
import org.apache.jena.reasoner.ValidityReport;
import org.apache.jena.util.FileManager;
import org.apache.jena.util.PrintUtil;
import org.apache.jena.vocabulary.OWL;
import org.apache.jena.vocabulary.OWL2;
import org.apache.jena.vocabulary.RDF;
import org.apache.jena.vocabulary.RDFS;

import java.io.*;
import java.util.*;



public class reasoner {
    Model schema;
    Model schemaYago;
    Model data;
    Model dataYago;
    InfModel infmodel;
    InfModel infmodelYago;
    Reasoner reasoner;
    Reasoner reasonerYago;


    reasoner(String ontologyschema, String ontologydata,String ontologyformat) throws FileNotFoundException {
        schema = FileManager.get().loadModel("src/main/resources/"+ontologyschema);
        System.out.println("Schema created");
        data = ModelFactory.createDefaultModel();
        System.out.println("Data Loaded");
        data.read(new FileInputStream(new File("src/main/resources/"+ontologydata)),null,ontologyformat);
        reasoner = ReasonerRegistry.getOWLReasoner();
        reasoner = reasoner.bindSchema(schema);
        infmodel = ModelFactory.createInfModel(reasoner, data);
        System.out.println("Model created");
    }

    private void printStatements(Model m, Resource s, Property p, Resource o) {
        for (StmtIterator i = m.listStatements(s,p,o); i.hasNext(); ) {
            Statement stmt = i.nextStatement();
            System.out.println(" - " + PrintUtil.print(stmt));
        }
    }

    public ArrayList<Statement> getStatements(Model m, Resource s, Property p, Resource o) {
        ArrayList<Statement> list = new ArrayList<Statement>();
        for (StmtIterator i = m.listStatements(s,p,o); i.hasNext(); ) {
            list.add(i.nextStatement());
        }
        return list;
    }
    public ArrayList<Statement> getDifferentPropertyStatements(Model m, Resource s, Property p, Resource o) {
        ArrayList<Statement> list = new ArrayList<Statement>();
        for (StmtIterator i = m.listStatements(s,null,o); i.hasNext(); ) {
            if(!i.nextStatement().getPredicate().equals(p)) {
                list.add((Statement) i);
            }
        }

        return list;
    }

    public void rangeDomain() {
        HashSet<Resource> cls_set = new HashSet<Resource>();
        ArrayList<Statement> ls = getStatements(data, null, RDF.type, null);
        for(Statement i : ls) {
            Resource cls = i.getObject().asResource();
            cls_set.add(cls);
        }

        HashSet<Property> p_set = new HashSet<Property>();
        for (StmtIterator i = data.listStatements(); i.hasNext(); ) {
            Property p =  i.nextStatement().getPredicate();
            p_set.add(p);
        }

        ArrayList<String> ranges = new ArrayList<String>();
        ArrayList<String> domains = new ArrayList<String>();
        int count = 0;
        for(Iterator<Property> i = p_set.iterator(); i.hasNext(); ) {
            Property p = i.next();
            float perc = (float) count++/p_set.size();
            System.out.println(p + " " + perc);
            //System.out.println("RANGE");
            ArrayList<Statement> l = getStatements(schema, p, RDFS.range, null);
            for (Statement t : l) {
                Resource range = t.getObject().asResource();

                //ArrayList<Statement> sub_range = getStatements(infmodel, null, RDFS.subClassOf, range);
                ArrayList<Statement> sub_range = getStatements(schema, null, RDFS.subClassOf, range);
                for (Statement tmp : sub_range) {
                    if (cls_set.contains(tmp.getSubject())) {
                        System.out.println(p.getLocalName() + " RANGE " + tmp.getSubject().getLocalName());
                        ranges.add(p.getURI() + "\t" + tmp.getSubject().getURI());
                    }
                }
            }

            l = getStatements(schema, p, RDFS.domain, null);
            for (Statement t : l) {
                Resource range = t.getObject().asResource();
                //ArrayList<Statement> sub_domain = getStatements(infmodel, null, RDFS.subClassOf, range);
                ArrayList<Statement> sub_domain = getStatements(schema, null, RDFS.subClassOf, range);
                for (Statement tmp : sub_domain) {
                    if (cls_set.contains(tmp.getSubject())) {
                        System.out.println(p.getLocalName() + " DOMAIN " + tmp.getSubject().getLocalName());
                        domains.add(p.getURI() + "\t" + tmp.getSubject().getURI());
                    }
                }
            }
        }
        try {
            FileOutputStream fos = new FileOutputStream("ranges.ser");
            ObjectOutputStream oos = new ObjectOutputStream(fos);
            oos.writeObject(ranges);
            oos.close();
            fos.close();
            System.out.println("Serialized HashMap data is saved in ranges.ser");
        }catch(IOException ioe) {
            ioe.printStackTrace();
        }
        try {
            FileOutputStream fos = new FileOutputStream("domains.ser");
            ObjectOutputStream oos = new ObjectOutputStream(fos);
            oos.writeObject(domains);
            oos.close();
            fos.close();
            System.out.println("Serialized HashMap data is saved in domains.ser");
        }catch(IOException ioe) {
            ioe.printStackTrace();
        }
    }

    public void generateDisjointClass() {
        HashSet<Resource> cls_set = new HashSet<Resource>();
        ArrayList<Statement> l = getStatements(data, null, RDF.type, null);
        for(Statement i : l) {
            Resource cls = i.getObject().asResource();
            cls_set.add(cls);
        }
        ArrayList<String> clsToDisj = new ArrayList<String>();
        int count = 0;
        for( Iterator<Resource> i = cls_set.iterator(); i.hasNext(); ) {
            Resource next = i.next();
            float perc = (float) ++count/cls_set.size();
            System.out.println(next.getURI() + " " + perc);
            //for(Statement t : getStatements(infmodel, next, OWL.disjointWith, null)) {
            for(Statement t : getStatements(schema, next, OWL.disjointWith, null)) {
                Resource disj = t.getObject().asResource();
                //for(Statement t2 : getStatements(infmodel, null, RDFS.subClassOf, disj)) {
                for(Statement t2 : getStatements(schema, null, RDFS.subClassOf, disj)) {
                    Resource sub_disj = t2.getSubject();
                    if(cls_set.contains(sub_disj)) {
                        System.out.println("\t" + next.getLocalName() + " DisjointWith " + sub_disj.getLocalName());
                        String s = next.getURI() + "\t" + sub_disj.getURI();
                        clsToDisj.add(s);
                    }
                }
            }
        }
        try {
            FileOutputStream fos = new FileOutputStream("disjoint.ser");
            ObjectOutputStream oos = new ObjectOutputStream(fos);
            oos.writeObject(clsToDisj);
            oos.close();
            fos.close();
            System.out.println("Serialized HashMap data is saved in hashmap.ser");
        }catch(IOException ioe) {
            ioe.printStackTrace();
        }
    }

    public void generateEquivalentClass() {
        ArrayList<String> out=new ArrayList<String>();
        ArrayList<Statement> l = getStatements(schema, null,OWL.equivalentClass, null);
        for(Statement i : l) {
                out.add("<" + i.getSubject() + "> <" + i.getObject().asResource() + ">");
        }

        try {
            saveList(out, "equivalentClass.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }
    public void generateInverseOfClass() {
        ArrayList<String> out=new ArrayList<String>();
        ArrayList<Statement> l = getStatements(schema, null,OWL.inverseOf, null);
        for(Statement i : l) {
            out.add("<" + i.getSubject() + "> <"+ i.getObject().asResource() + ">");
        }

        try {
            saveList(out, "inverseOf.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }
    public void generateFunctionalPropertyClass() {
        ArrayList<String> out=new ArrayList<String>();
        ArrayList<Statement> l = getStatements(schema, null,RDF.type, OWL.FunctionalProperty);
        for(Statement i : l) {
            out.add("<" + i.getSubject() +  ">");
        }

        try {
            saveList(out, "functionalProperty.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }
    public void generateEquivalentPropertyClass() {
        ArrayList<String> out=new ArrayList<String>();
        ArrayList<Statement> l = getStatements(schema, null,OWL.equivalentProperty, null);
        for(Statement i : l) {
            out.add("<" + i.getSubject() + "> <" + i.getObject().asResource() + ">");
        }

        try {
            saveList(out, "equivalentProperty.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }

    public void generateSubClassOf() {
        ArrayList<String> out=new ArrayList<String>();
        ArrayList<Statement> l = getStatements(schema, null,RDFS.subClassOf, null);
        for(Statement i : l) {
            out.add("<" + i.getSubject() + "> <" + i.getObject().asResource() + ">");
        }

        try {
            saveList(out, "subclassof.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }

    public void corruptClass() {
        Multimap<String, String> disj_map = ArrayListMultimap.create();
        for(String in : openSerial("disjoint.ser")) {
            Scanner s = new Scanner(in).useDelimiter("\t");
            disj_map.put(s.next(),s.next());
        }

        ArrayList<String> out = new ArrayList<String>();
        ArrayList<Statement> ls = getStatements(data, null, RDF.type, null);
        for(Statement i : ls) {
            Resource cls = i.getObject().asResource();
            for (String s : disj_map.get(cls.getURI())) {
                out.add("<" + i.getSubject() + "> <" + s + "> <" + i.getPredicate() + ">");
            }
        }

        try {
            saveList(out,"corruptTypeOf.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }

    }

    private void print(Object s) {
        System.out.println(s);
    }

    private void saveList(List<String> out, String outName) throws FileNotFoundException, UnsupportedEncodingException {
        PrintWriter writer = new PrintWriter(outName, "UTF-8");
        for(String s : out) {
            writer.println(s);
        }
        writer.close();
        print("List saved on " + outName);
    }

    private ArrayList<String> openSerial(String name) {
        ArrayList<String> serialized = null;
        try {
            FileInputStream fis = new FileInputStream(name);
            ObjectInputStream ois = new ObjectInputStream(fis);
            serialized = (ArrayList) ois.readObject();
            ois.close();
            fis.close();
        }catch(IOException ioe) {
            ioe.printStackTrace();
        } catch(ClassNotFoundException c) {
            System.out.println("Class not found");
            c.printStackTrace();
        }
        System.out.println("Deserialized HashMap");
        return serialized;
    }

    public void getRangeDomainFromSer() {
        ArrayList<String> strings = openSerial("ranges.ser");
        ArrayList<String> strings1 = openSerial("domains.ser");

        ArrayList<String> range = new ArrayList<String>();
        ArrayList<String> domain = new ArrayList<String>();

        for(String s : strings) {
            Scanner sc = new Scanner(s).useDelimiter("\t");
            range.add("<" + sc.next() + ">\t<" + sc.next() + ">");
        }

        for(String s : strings1) {
            Scanner sc = new Scanner(s).useDelimiter("\t");
            domain.add("<" + sc.next() + ">\t<" + sc.next() + ">");
        }

        try {
            saveList(range, "rs_range.txt");
            saveList(domain, "rs_domain.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }

    public void yagoMerge(String yagoschema,String yagodatalinks,String yagodata,String linksschema,String yagodataschema) throws FileNotFoundException, UnsupportedEncodingException {

        schemaYago = FileManager.get().loadModel("src/main/resources/"+yagoschema);
        dataYago = ModelFactory.createDefaultModel();
        dataYago.read(new FileInputStream(new File("src/main/resources/" + yagodatalinks)),null,linksschema);
        dataYago.read(new FileInputStream(new File("src/main/resources/" + yagodata)),null,yagodataschema);
        reasonerYago = ReasonerRegistry.getOWLReasoner();
        reasonerYago = reasonerYago.bindSchema(schemaYago);
        infmodelYago = ModelFactory.createInfModel(reasonerYago, dataYago);

        HashSet<Resource> sameas_obj_set = new HashSet<Resource>();
        HashSet<Resource> sameas_sub_set = new HashSet<Resource>();
        HashSet<Resource> db_entities = new HashSet<Resource>();
        HashSet<Resource> yago_entities= new HashSet<Resource>();
        HashSet<Resource> yago_total_entities= new HashSet<Resource>();
        ArrayList<Couple> db_yago=new ArrayList<Couple>();
        ArrayList<Statement> yago_triples= new ArrayList<Statement>();
        ArrayList<Statement> yago_triples_total= new ArrayList<Statement>();
        ArrayList<Statement> ls_db_triples= getStatements(data, null, null, null);

        for(Statement i : ls_db_triples){
            db_entities.add(i.getSubject().asResource());
            db_entities.add(i.getObject().asResource());
        }
        for(Resource j : db_entities){
            ArrayList<Statement> tmp= getStatements(dataYago, j, OWL.sameAs, null);
            if(tmp.size()!=0) {
                Iterator<Statement> tm = tmp.iterator();
                yago_entities.add(tm.next().getObject().asResource());
            }
        }

        for(Resource k : yago_entities){
            ArrayList<Statement> tmp= getStatements(dataYago, k, null, null);
            yago_triples.addAll(tmp);
            tmp= getStatements(dataYago,null, null, k);
            yago_triples.addAll(tmp);
        }

        yago_triples_total.addAll(yago_triples);
        for(Statement l : yago_triples){
            ArrayList<Statement> tmp= getStatements(dataYago, l.getSubject(), RDFS.subClassOf, null);
            yago_triples_total.addAll(tmp);
            tmp= getStatements(dataYago,null, RDFS.subClassOf, l.getObject().asResource());
            yago_triples_total.addAll(tmp);
        }

        System.out.println(yago_triples.size());

        ArrayList<String> out = new ArrayList<String>();
        ArrayList<String> out2 = new ArrayList<String>();
        for(Statement i : yago_triples_total) {
            Resource sub=i.getSubject().asResource();
            Resource obj=i.getObject().asResource();
            ArrayList<Statement> tmp= getStatements(dataYago, null, OWL.sameAs, i.getSubject());
            if(tmp.size()!=0) {
                Iterator<Statement> tm = tmp.iterator();
                sub=tm.next().getSubject().asResource();

            }
            tmp= getStatements(dataYago, null, OWL.sameAs, i.getObject().asResource());
            if(tmp.size()!=0) {
                Iterator<Statement> tm = tmp.iterator();
                obj=tm.next().getSubject().asResource();
            }
            if(!i.getPredicate().equals(OWL.sameAs)){
                out.add("<" + sub + "> <" + i.getPredicate() + "> <" + obj + ">");
                out2.add("<" + sub + "> <" + i.getPredicate() + "> <" + obj + ">.");
            }
        }

        try {
            saveList(out, "merge.txt");
            saveList(out2, "merge.ttl");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }

    }

    public void class2id(){
        HashSet<Resource> db_classes = new HashSet<Resource>();
        ArrayList<String> out = new ArrayList<String>();
        ArrayList<Statement> all_typeof= getStatements(data,null,RDF.type,null);
        for (Statement i : all_typeof){
            db_classes.add(i.getObject().asResource());
        }
        ArrayList<Statement> all_subclass= getStatements(schema,null,RDFS.subClassOf,null);
        for (Statement i : all_subclass){
            db_classes.add(i.getObject().asResource());
            db_classes.add(i.getSubject().asResource());
        }
        int k=0;
        for(Resource j : db_classes){
            out.add("<" + j + "> " + k);
            k++;
        }

        try {
            saveList(out, "class2id.txt");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }

    }




    class Tripla{
        Resource subject;
        Resource predicate;
        Resource object;

        Tripla(Resource sub,Resource pred,Resource obj){
            this.subject=sub;
            this.predicate=pred;
            this.object=obj;
        }
    }

    class Couple{
        Resource subject;
        Resource object;

        Couple(Resource sub,Resource obj){
            this.subject=sub;
            this.object=obj;
        }
    }
}
