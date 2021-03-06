#include "myreaction.h"

MyReaction::MyReaction () {}

MyReaction::~MyReaction () 
{
  for (int i=0; i<listOfMyReactants.size (); i++)
	delete listOfMyReactants[i];
  for (int i=0; i<listOfMyModifiers.size (); i++)
	delete listOfMyModifiers[i];
  for (int i=0; i<listOfMyProducts.size (); i++)
	delete listOfMyProducts[i];
}

void MyReaction::completeReaction (
	vector<MyCompartment*>& listOfMyCompartments,
	vector<MySpecies*>& listOfMySpecies,
	vector<MySpecies*>& productsBody,
	const reactionTemplate::speciesArrayMatch& reactantsM,
	const reactionTemplate::speciesArrayMatch& modifiersM,
	const reactionTemplate* RT
	)
{
  //(1) mix species 
  MySpecies* sMix = new MySpecies;
  map<string,string> replaceTable;

  //  (1.1) copy modifiers
  for (int i=0; i < modifiersM.size (); i++)
  {
	int sind = modifiersM[i].first;
	MySpecies* mr = listOfMySpecies[sind];
	listOfMyModifiers.push_back (mr);

	string Label = RT->listOfMyModifiers[i]->getLabel ();
	replaceTable[Label] = mr->getId ();
  }

  //  (1.2) copy and mix reactants
  for (int i=0; i < reactantsM.size (); i++)
  {
	int sind = reactantsM[i].first;
	reactionTemplate::cMatchsType2 speciesM = reactantsM[i].second;

	MySpecies* sr = listOfMySpecies[sind];
	listOfMyReactants.push_back (sr);

	//	complete chainUsed
	set<int> chainUsed;
	for (int j=0; j < speciesM.size ();j++)
	  chainUsed.insert (speciesM[j].second);

	//	copy chains that contributes to matching
	for (int j=0; j < sr->getNumOfChains (); j++)
	{
	  if (!chainUsed.count (j)) 
		sMix->createChain (sr->getChain (j));
	}

	//	copy all trees
	for (int j=0; j < sr->getNumOfTrees (); j++)
	  sMix->createTree (sr->getTree (j));

	string Label = RT->listOfMyReactants[i]->getLabel ();
	replaceTable[Label] = sr->getId ();
  }

  //  (1.3) copy and mix products
  int numProd = listOfMyProducts.size ();
  for (int i=0; i < productsBody.size () ; i++)
  {
	MySpecies* sp = productsBody[i];
	listOfMyProducts.push_back (sp);

	for (int j=0; j< sp->getNumOfChains (); j++)
	  sMix->createChain (sp->getChain (j));

	for (int j=0; j< sp->getNumOfTrees (); j++)
	  sMix->createTree (sp->getTree (j));
  }

  //  split species
  vector<MySpecies*> realProducts; 
  sMix->split (realProducts);

  //  readProducts are just copies
  delete sMix;

  //  add products
  vector<int>* dGp = new vector<int> [realProducts.size ()];

  for (int i=0; i< realProducts.size (); i++)
  {
	MySpecies* s = realProducts[i];

	set<string> chainLabel;
	for (int j =0; j < s->getNumOfChains (); j++)
	  chainLabel.insert (s->getChain (j)->getChainLabel ());

	vector<int> degenerate;
	for (int j =0; j < listOfMyProducts.size (); j++)
	{
	  MySpecies* tmS = listOfMyProducts[i];
	  for (int k=0; k < tmS->getNumOfChains (); k++)
	  {
		string label = tmS->getChain (k)->getChainLabel ();
		if (chainLabel.count (label))
		{
		  degenerate.push_back (j);
		  break;
		}
	  }
	}

	assert (degenerate.size () >= 1);
	dGp[i] = degenerate;
  }

  double* stoiMath = new double[listOfMyProducts.size ()];
	
  for (int i=0; i < realProducts.size (); i++)
  {
	MySpecies* s = realProducts[i];
	double stoi = 1.0/dGp[i].size ();
	
	//	for each species in listOfMyProducts
	for (int j=0; j < dGp[i].size (); j++)
	{
	  MySpecies* tmS = listOfMyProducts[dGp[i][j]];
	  stoiMath[dGp[i][j]] = stoi;
	  
	  //  new species
	  MySpecies* newsp (s);

	  //  set newsp identifier
	  ostringstream oss;
	  oss << "sPecIes" << listOfMySpecies.size ();
	  newsp->setId (oss.str ());

	  //  set compartment
	  string oldLabel = tmS->getLabel ();
	  if (RT->mmapIndexReactants.count (oldLabel))
	  {
		int index = RT->mmapIndexReactants.find(oldLabel)->second;
		newsp->setCompartment (
			listOfMyReactants[index]->getCompartment ()
			);
	  }
	  else if (RT->mmapIndexProducts.count (oldLabel))
	  {
		int index = RT->mmapIndexModifiers.find(oldLabel)->second;
		newsp->setCompartment (
			listOfMyModifiers[index]->getCompartment ()
			);
	  }
	  else
	  {
		string errno ("No Compartment found for Products!");
		throw errno;
	  }

	  //  add to speciesList
	  for (int k=0; k < listOfMyCompartments.size (); k++)
	  {
		MyCompartment* mycomp = listOfMyCompartments[k];
		if (mycomp->getId () == newsp->getCompartment ()) 
		{
		  MySpecies* sEx = mycomp->isMySpeciesIn (newsp);
		  if (sEx == NULL)
		  {
			newsp->rearrange ();
			listOfMySpecies.push_back (newsp);
			mycomp->addMySpeciesIn (newsp);
		  }
		  else
		  {
			delete newsp;
			newsp = sEx;
		  }
		  break;
		}
	  }

	  //
	  //  add replacement
	  //
	  replaceTable[tmS->getLabel ()] = newsp->getId ();


	  //  delete old species and replaced with new one
	  delete tmS;
	  tmS = newsp;
	}

	delete s;
  }

  delete dGp;

  //
  //  complete components of Reaction object needed for SBML
  //

  //  speciesReference and ModifierReference
  for (int i=0; i<listOfMyReactants.size (); i++)
  {
	SpeciesReference* spr = createReactant ();
	spr->setSpecies (listOfMyReactants[i]->getId ());
  }

  for (int i=0; i<listOfMyProducts.size (); i++)
  {
	SpeciesReference* spr = createProduct ();
	spr->setSpecies (listOfMyProducts[i]->getId ());
	spr->setStoichiometry (stoiMath[i]);
  }

  for (int i=0; i<listOfMyModifiers.size (); i++)
  {
	ModifierSpeciesReference* smr = createModifier ();
	smr->setSpecies (listOfMyModifiers[i]->getId ());
  }

  //  setKineticLaw
  KineticLaw* kl = createKineticLaw ();

  //  setLocalParameter
  for (int i=0; i < RT->listOfParameters.size (); i++)
  {
	kl->addParameter (RT->listOfParameters[i]);
  }

  //  setMath
  string mathXMLString = RT->math;

  map<string,string>::const_iterator itMath = replaceTable.begin ();
  while (itMath != replaceTable.end ())
  {
	string::size_type posP = mathXMLString.find (itMath->first);
	if (posP != string::npos)
	  mathXMLString.replace (
		  posP, itMath->first.size (), itMath->second
		  );
	itMath ++;
  }

  ASTNode* astMath = readMathMLFromString(mathXMLString.c_str());
  kl->setMath (astMath);
  delete astMath;

  delete stoiMath;
}


