#include <tadah/core/periodic_table.h>

namespace tadah {
namespace core {

// Definition of static members
std::map<std::string, Element> PeriodicTable::symbol;
std::map<std::string, Element> PeriodicTable::name;
std::map<int, Element> PeriodicTable::Z;

std::map<int, double> PeriodicTable::masses;

// Definitions of static methods
void PeriodicTable::initialize() {
  if (symbol.empty()) {
    fill();
  }
  if (masses.empty()) {
    init_masses();
  }
}

const Element &PeriodicTable::find_by_name(const std::string &s) {
  try {
    return name.at(s);
  } catch (const std::exception &e) {
    std::cerr << "Error in find_by_name: " << e.what() << " (name: '" << s << "')\n";
    throw;
  } catch (...) {
    std::cerr << "Unknown error in find_by_name (name: '" << s << "')\n";
    throw;
  }
}

const Element &PeriodicTable::find_by_symbol(const std::string &s) {
  try {
    return symbol.at(s);
  } catch (const std::out_of_range &e) {
    // Re-throw a std::runtime_error to match the test’s expectation
    throw std::runtime_error(std::string("Unknown symbol: ") + s);
  }
}
const Element &PeriodicTable::find_by_Z(int z) {
  try {
    return Z.at(z);
  } catch (const std::out_of_range &e) {
    // Re-throw a std::runtime_error to match the test’s expectation
    throw std::runtime_error("Unknown atomic number: " + std::to_string(z));
  }
}
double PeriodicTable::get_mass(int Zvalue) {
    // 1) If atomic number is beyond 118, throw a runtime_error instead of out_of_range.
    if (Zvalue > 118) {
        throw std::runtime_error("Atomic number Z > 118");
    }

    // 2) The rest remains the same, so if Zvalue <= 118 but not in the map we catch out_of_range.
    try {
        return masses.at(Zvalue);
    } catch (const std::out_of_range &) {
        std::cerr << "Error: Mass for atomic number " << Zvalue << " not found.\n";
        throw;
    } catch (...) {
        std::cerr << "Unknown error in get_mass for atomic number " << Zvalue << ".\n";
        throw;
    }
}

double PeriodicTable::get_mass(const Element &e) {
  try {
    return get_mass(e.Z);
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << " while getting mass for element with symbol '" 
      << e.symbol << "' and atomic number " << e.Z << ".\n";
    throw;
  } catch (...) {
    std::cerr << "Unknown error in get_mass for element with symbol '" << e.symbol
      << "' and atomic number " << e.Z << ".\n";
    throw;
  }
}

void PeriodicTable::add(const Element &e) {
  try {
    symbol[e.symbol] = e;
    Z[e.Z] = e;
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << " while adding element with symbol '"
      << e.symbol << "' and atomic number " << e.Z << ".\n";
    throw;
  } catch (...) {
    std::cerr << "Unknown error while adding element with symbol '" << e.symbol
      << "' and atomic number " << e.Z << ".\n";
    throw;
  }
}

void PeriodicTable::add(const std::string &symbol_, const std::string &name_, int Z_) {
  try {
    Element e(symbol_, Z_);
    symbol[symbol_] = e;
    name[name_] = e;
    Z[Z_] = e;
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << " while adding element with symbol '"
      << symbol_ << "', name '" << name_ << "', and atomic number " << Z_ << ".\n";
    throw;
  } catch (...) {
    std::cerr << "Unknown error while adding element with symbol '" << symbol_
      << "', name '" << name_ << "', and atomic number " << Z_ << ".\n";
    throw;
  }
}

void PeriodicTable::fill() {
  add("H","Hydrogen",1);
  add("He","Helium",2);
  add("Li","Lithium",3);
  add("Be","Beryllium",4);
  add("B","Boron",5);
  add("C","Carbon",6);
  add("N","Nitrogen",7);
  add("O","Oxygen",8);
  add("F","Fluorine",9);
  add("Ne","Neon",10);
  add("Na","Sodium",11);
  add("Mg","Magnesium",12);
  add("Al","Aluminium",13);
  add("Si","Silicon",14);
  add("P","Phosphorus",15);
  add("S","Sulfur",16);
  add("Cl","Chlorine",17);
  add("Ar","Argon",18);
  add("K","Potassium",19);
  add("Ca","Calcium",20);
  add("Sc","Scandium",21);
  add("Ti","Titanium",22);
  add("V","Vanadium",23);
  add("Cr","Chromium",24);
  add("Mn","Manganese",25);
  add("Fe","Iron",26);
  add("Co","Cobalt",27);
  add("Ni","Nickel",28);
  add("Cu","Copper",29);
  add("Zn","Zinc",30);
  add("Ga","Gallium",31);
  add("Ge","Germanium",32);
  add("As","Arsenic",33);
  add("Se","Selenium",34);
  add("Br","Bromine",35);
  add("Kr","Krypton",36);
  add("Rb","Rubidium",37);
  add("Sr","Strontium",38);
  add("Y","Yttrium",39);
  add("Zr","Zirconium",40);
  add("Nb","Niobium",41);
  add("Mo","Molybdenum",42);
  add("Tc","Technetium",43);
  add("Ru","Ruthenium",44);
  add("Rh","Rhodium",45);
  add("Pd","Palladium",46);
  add("Ag","Silver",47);
  add("Cd","Cadmium",48);
  add("In","Indium",49);
  add("Sn","Tin",50);
  add("Sb","Antimony",51);
  add("Te","Tellurium",52);
  add("I","Iodine",53);
  add("Xe","Xenon",54);
  add("Cs","Caesium",55);
  add("Ba","Barium",56);
  add("La","Lanthanum",57);
  add("Ce","Cerium",58);
  add("Pr","Praseodymium",59);
  add("Nd","Neodymium",60);
  add("Pm","Promethium",61);
  add("Sm","Samarium",62);
  add("Eu","Europium",63);
  add("Gd","Gadolinium",64);
  add("Tb","Terbium",65);
  add("Dy","Dysprosium",66);
  add("Ho","Holmium",67);
  add("Er","Erbium",68);
  add("Tm","Thulium",69);
  add("Yb","Ytterbium",70);
  add("Lu","Lutetium",71);
  add("Hf","Hafnium",72);
  add("Ta","Tantalum",73);
  add("W","Tungsten",74);
  add("Re","Rhenium",75);
  add("Os","Osmium",76);
  add("Ir","Iridium",77);
  add("Pt","Platinum",78);
  add("Au","Gold",79);
  add("Hg","Mercury",80);
  add("Tl","Thallium",81);
  add("Pb","Lead",82);
  add("Bi","Bismuth",83);
  add("Po","Polonium",84);
  add("At","Astatine",85);
  add("Rn","Radon",86);
  add("Fr","Francium",87);
  add("Ra","Radium",88);
  add("Ac","Actinium",89);
  add("Th","Thorium",90);
  add("Pa","Protactinium",91);
  add("U","Uranium",92);
  add("Np","Neptunium",93);
  add("Pu","Plutonium",94);
  add("Am","Americium",95);
  add("Cm","Curium",96);
  add("Bk","Berkelium",97);
  add("Cf","Californium",98);
  add("Es","Einsteinium",99);
  add("Fm","Fermium",100);
  add("Md","Mendelevium",101);
  add("No","Nobelium",102);
  add("Lr","Lawrencium",103);
  add("Rf","Rutherfordium",104);
  add("Db","Dubnium",105);
  add("Sg","Seaborgium",106);
  add("Bh","Bohrium",107);
  add("Hs","Hassium",108);
  add("Mt","Meitnerium",109);
  add("Ds","Darmstadtium",110);
  add("Rg","Roentgenium",111);
  add("Cn","Copernicium",112);
  add("Nh","Nihonium",113);
  add("Fl","Flerovium",114);
  add("Mc","Moscovium",115);
  add("Lv","Livermorium",116);
  add("Ts","Tennessine",117);
  add("Og","Oganessian",118);
}
void PeriodicTable::init_masses() {
  masses[1] = 1.00800;    // Hydrogen
  masses[2] = 4.00260;    // Helium
  masses[3] = 6.94000;    // Lithium
  masses[4] = 9.01218;    // Beryllium
  masses[5] = 10.8100;    // Boron
  masses[6] = 12.0110;    // Carbon
  masses[7] = 14.0070;    // Nitrogen
  masses[8] = 15.9990;    // Oxygen
  masses[9] = 18.9984;    // Fluorine
  masses[10] = 20.1800;   // Neon
  masses[11] = 22.9898;   // Sodium
  masses[12] = 24.3050;   // Magnesium
  masses[13] = 26.9815;   // Aluminum
  masses[14] = 28.0850;   // Silicon
  masses[15] = 30.9738;   // Phosphorus
  masses[16] = 32.0600;   // Sulfur
  masses[17] = 35.4500;   // Chlorine
  masses[18] = 39.9480;   // Argon
  masses[19] = 39.0983;   // Potassium
  masses[20] = 40.0780;   // Calcium
  masses[21] = 44.9559;   // Scandium
  masses[22] = 47.8670;   // Titanium
  masses[23] = 50.9415;   // Vanadium
  masses[24] = 51.9961;   // Chromium
  masses[25] = 54.9380;   // Manganese
  masses[26] = 55.8450;   // Iron
  masses[27] = 58.9332;   // Cobalt
  masses[28] = 58.6934;   // Nickel
  masses[29] = 63.5460;   // Copper
  masses[30] = 65.3800;   // Zinc
  masses[31] = 69.7230;   // Gallium
  masses[32] = 72.6300;   // Germanium
  masses[33] = 74.9216;   // Arsenic
  masses[34] = 78.9710;   // Selenium
  masses[35] = 79.9040;   // Bromine
  masses[36] = 83.7980;   // Krypton
  masses[37] = 85.4678;   // Rubidium
  masses[38] = 87.6200;   // Strontium
  masses[39] = 88.9058;   // Yttrium
  masses[40] = 91.2240;   // Zirconium
  masses[41] = 92.9064;   // Niobium
  masses[42] = 95.9500;   // Molybdenum
  masses[43] = 98.0000;   // Technetium
  masses[44] = 101.070;   // Ruthenium
  masses[45] = 102.905;   // Rhodium
  masses[46] = 106.420;   // Palladium
  masses[47] = 107.868;   // Silver
  masses[48] = 112.414;   // Cadmium
  masses[49] = 114.818;   // Indium
  masses[50] = 118.710;   // Tin
  masses[51] = 121.760;   // Antimony
  masses[52] = 127.600;   // Tellurium
  masses[53] = 126.904;   // Iodine
  masses[54] = 131.293;   // Xenon
  masses[55] = 132.905;   // Cesium
  masses[56] = 137.327;   // Barium
  masses[57] = 138.905;   // Lanthanum
  masses[58] = 140.116;   // Cerium
  masses[59] = 140.907;   // Praseodymium
  masses[60] = 144.242;   // Neodymium
  masses[61] = 145.000;   // Promethium
  masses[62] = 150.360;   // Samarium
  masses[63] = 151.964;   // Europium
  masses[64] = 157.250;   // Gadolinium
  masses[65] = 158.925;   // Terbium
  masses[66] = 162.500;   // Dysprosium
  masses[67] = 164.930;   // Holmium
  masses[68] = 167.259;   // Erbium
  masses[69] = 168.934;   // Thulium
  masses[70] = 173.045;   // Ytterbium
  masses[71] = 174.966;   // Lutetium
  masses[72] = 178.490;   // Hafnium
  masses[73] = 180.948;   // Tantalum
  masses[74] = 183.840;   // Tungsten
  masses[75] = 186.207;   // Rhenium
  masses[76] = 190.230;   // Osmium
  masses[77] = 192.217;   // Iridium
  masses[78] = 195.084;   // Platinum
  masses[79] = 196.967;   // Gold
  masses[80] = 200.592;   // Mercury
  masses[81] = 204.380;   // Thallium
  masses[82] = 207.200;   // Lead
  masses[83] = 208.980;   // Bismuth
  masses[84] = 209.000;   // Polonium
  masses[85] = 210.000;   // Astatine
  masses[86] = 222.000;   // Radon
  masses[87] = 223.000;   // Francium
  masses[88] = 226.000;   // Radium
  masses[89] = 227.000;   // Actinium
  masses[90] = 232.038;   // Thorium
  masses[91] = 231.035;   // Protactinium
  masses[92] = 238.029;   // Uranium
  masses[93] = 237.000;   // Neptunium
  masses[94] = 244.000;   // Plutonium
  masses[95] = 243.000;   // Americium
  masses[96] = 247.000;   // Curium
  masses[97] = 247.000;   // Berkelium
  masses[98] = 251.000;   // Californium
  masses[99] = 252.000;   // Einsteinium
  masses[100] = 257.000;  // Fermium
  masses[101] = 258.000;  // Mendelevium
  masses[102] = 259.000;  // Nobelium
  masses[103] = 262.000;  // Lawrencium
  masses[104] = 267.000;  // Rutherfordium
  masses[105] = 270.000;  // Dubnium
  masses[106] = 271.000;  // Seaborgium
  masses[107] = 270.000;  // Bohrium
  masses[108] = 277.000;  // Hassium
  masses[109] = 278.000;  // Meitnerium
  masses[110] = 281.000;  // Darmstadtium
  masses[111] = 282.000;  // Roentgenium
  masses[112] = 285.000;  // Copernicium
  masses[113] = 286.000;  // Nihonium
  masses[114] = 289.000;  // Flerovium
  masses[115] = 290.000;  // Moscovium
  masses[116] = 293.000;  // Livermorium
  masses[117] = 294.000;  // Tennessine
  masses[118] = 294.000;  // Oganesson
}

} // namespace core
} // namespace tadah

