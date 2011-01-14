/*
KeccakTools

The Keccak sponge function, designed by Guido Bertoni, Joan Daemen,
Michaël Peeters and Gilles Van Assche. For more information, feedback or
questions, please refer to our website: http://keccak.noekeon.org/

Implementation by the designers,
hereby denoted as "the implementer".

To the extent possible under law, the implementer has waived all copyright
and related or neighboring rights to the source code in this file.
http://creativecommons.org/publicdomain/zero/1.0/
*/

#include <iostream>
#include <sstream>
#include "Keccak-fCodeGen.h"

using namespace std;

KeccakFCodeGen::KeccakFCodeGen(unsigned int aWidth, unsigned int aNrRounds)
    : KeccakF(aWidth, aNrRounds), interleavingFactor(1),
    wordSize(laneSize), outputMacros(false), scheduleType(1)
{
}

void KeccakFCodeGen::setInterleavingFactor(unsigned int anInterleavingFactor)
{
    interleavingFactor = anInterleavingFactor;
    wordSize = laneSize/interleavingFactor;
}

void KeccakFCodeGen::setOutputMacros(bool anOutputMacros)
{
    outputMacros = anOutputMacros;
}

void KeccakFCodeGen::setScheduleType(unsigned int aScheduleType)
{
    if ((aScheduleType >= 1) && (aScheduleType <= 2))
        scheduleType = aScheduleType;
}

void KeccakFCodeGen::displayRoundConstants()
{
    for(unsigned int i=0; i<roundConstants.size(); ++i) {
        cout << "KeccakF" << dec << width << "RoundConstants[";
        cout.width(2); cout.fill(' ');
        cout << dec << i << "] = ";
        cout.width(16); cout.fill('0');
        cout << hex << roundConstants[i] << endl;
    }
}

void KeccakFCodeGen::displayRhoOffsets(bool moduloWordLength)
{
    int offset = 2;

    cout << "ρ:" << endl;
    cout << "col  |";
    for(unsigned int sx=0; sx<5; sx++) {
        unsigned int x = index(sx-offset);
        cout.width(4); cout.fill(' ');
        cout << dec << x;
    }
    cout << endl;
    cout << "-----+--------------------" << endl;
    for(unsigned int sy=0; sy<5; sy++) {
        unsigned int y = index(5-1-sy-offset);
        cout << dec << "row " << dec << y << "|";
        for(unsigned int sx=0; sx<5; sx++) {
            unsigned int x = index(sx-offset);
            cout.width(4); cout.fill(' ');
            if (moduloWordLength)
                cout << dec << (rhoOffsets[index(x, y)] % laneSize);
            else
                cout << dec << (rhoOffsets[index(x, y)]);
        }
        cout << endl;
    }
    cout << endl;
}

void KeccakFCodeGen::displayPi()
{
    for(unsigned int x=0; x<5; x++)
    for(unsigned int y=0; y<5; y++) {
        cout << dec << "(" << x << "," << y << ") goes to ";
        unsigned int X, Y;
        pi(x, y, X, Y);
        cout << dec << "(" << X << "," << Y << ").";
        cout << endl;
    }
}

string KeccakFCodeGen::buildWordName(const string& prefixSymbol, unsigned int x, unsigned int y, unsigned int z) const
{
    return buildBitName(laneName(prefixSymbol, x, y), interleavingFactor, z);
}

string KeccakFCodeGen::buildWordName(const string& prefixSymbol, unsigned int x, unsigned int z) const
{
    return buildBitName(sheetName(prefixSymbol, x), interleavingFactor, z);
}

void KeccakFCodeGen::genDeclarations(ostream& fout) const
{
    genDeclarationsLanes(fout, "A");
    genDeclarationsLanes(fout, "B");
    genDeclarationsSheets(fout, "C");
    genDeclarationsSheets(fout, "D");
    genDeclarationsLanes(fout, "E");
    fout << endl;
}

void KeccakFCodeGen::genDeclarationsLanes(ostream& fout, const string& prefixSymbol) const
{
    for(unsigned int y=0; y<5; y++)
    for(unsigned int z=0; z<interleavingFactor; z++) {
        fout << "    ";
        fout << (outputMacros ? "V" : "UINT") << dec << wordSize << " ";
        for(unsigned int x=0; x<5; x++) {
            fout << buildWordName(prefixSymbol, x, y, z);
            if (x < 4)
                fout << ", ";
        }
        fout << "; \\" << endl;
    }
}

void KeccakFCodeGen::genDeclarationsSheets(ostream& fout, const string& prefixSymbol) const
{
    for(unsigned int z=0; z<interleavingFactor; z++) {
        fout << "    ";
        fout << (outputMacros ? "V" : "UINT") << dec << wordSize << " ";
        for(unsigned int x=0; x<5; x++) {
            fout << buildWordName(prefixSymbol, x, z);
            if (x < 4)
                fout << ", ";
        }
        fout << "; \\" << endl;
    }
}

unsigned int KeccakFCodeGen::schedule(unsigned int i) const
{
    if (scheduleType == 1)
        return i;
    else if (scheduleType == 2) {
        const unsigned int theSchedule[10] = { 0, 1, 2, 5, 3, 6, 4, 7, 8, 9 };
        return theSchedule[i];
    }
    else
        return i;
}

void KeccakFCodeGen::genCodeForRound(ostream& fout, bool prepareTheta, 
                                     SliceValue inChiMask, SliceValue outChiMask, 
                                     string A, string B, string C,
                                     string D, string E, string header) const
{
    fout << "// --- Code for round";
    if (prepareTheta) fout << ", with prepare-theta";
    if (outChiMask != 0) {
        fout << " (lane complementing pattern '";
        for(unsigned int y=0; y<5; y++)
        for(unsigned int x=0; x<5; x++) {
            if (getRowFromSlice(outChiMask, y) & (1<<x))
                fout << laneName("", x, y);
        }
        fout << "')";
    }
    fout << endl;
    fout << "// --- ";
    if (interleavingFactor > 1)
        fout << "using factor " << interleavingFactor << " interleaving, ";
    fout << dec << laneSize << "-bit lanes mapped to " << wordSize << "-bit words";
    fout << endl;

    if (header.size() > 0)
        fout << header << endl;

    for(unsigned int x=0; x<5; x++)
    for(unsigned int Z=0; Z<interleavingFactor; Z++) {
        unsigned int r = 1;
        unsigned int rMod = r%interleavingFactor;
        unsigned int rDiv = r/interleavingFactor;
        unsigned int z = (interleavingFactor + Z - rMod)%interleavingFactor;
        unsigned int rInt = rDiv + (z + rMod)/interleavingFactor;
        // From C to D      
        fout << "    " << buildWordName(D, x, Z) << " = ";
        fout << strXOR(buildWordName(C, (x+4)%5, Z), strROL(buildWordName(C, (x+1)%5, z), rInt));
        fout << "; \\" << endl;
    }

    fout << "\\" << endl;

    for(unsigned int Y=0; Y<5; Y++)
    for(unsigned int Z=0; Z<interleavingFactor; Z++) {
        for(unsigned int i=0; i<10; i++)
        for(unsigned int X=0; X<5; X++) {
            unsigned int x, y;
            inversePi(X, Y, x, y);
            unsigned int r = rhoOffsets[index(x, y)] % laneSize;
            unsigned int rMod = r%interleavingFactor;
            unsigned int rDiv = r/interleavingFactor;
            unsigned int z = (interleavingFactor + Z - rMod)%interleavingFactor;
            unsigned int rInt = rDiv + (z + rMod)/interleavingFactor;
            unsigned j = schedule(i);

            if (j == X) {
                // θ
                fout << "    ";
                fout << strXOReq(buildWordName(A, x, y, z), buildWordName(D, x, z));
                fout << "; \\" << endl;

                // ρ then π
                fout << "    " << buildWordName(B, X, Y, Z) << " = ";
                fout << strROL(buildWordName(A, x, y, z), rInt);
                fout << "; \\" << endl;
            }

            if (j == (X+5)) {
                bool M0 = ((((outChiMask ^ inChiMask)>>(X+5*Y))&1) == 1);
                bool M1 = (((inChiMask>>((X+1)%5 + 5*Y))&1) == 1);
                bool M2 = (((inChiMask>>((X+2)%5 + 5*Y))&1) == 1);
                bool LC1 = (M1==M2) && (M0 == M1);
                bool LC2 = (M1==M2) && (M0 != M1);
                bool LOR = ((!M1) && M2) || (M0 && (M1==M2));
                bool LC0 = !LOR==M0; 
                // χ
                fout << "    " << buildWordName(E, X, Y, Z) << " = ";
                fout << strXOR(
                    strNOT(buildWordName(B, X, Y, Z), LC0),
                    strANDORnot(buildWordName(B, index(X+1), Y, Z), buildWordName(B, index(X+2), Y, Z), LC1, LC2, LOR));
                fout << "; \\" << endl;

                if ((X == 0) && (Y == 0)) {
                    // ι
                    stringstream str;
                    str << "KeccakF" << width << "RoundConstants";
                    if (interleavingFactor > 1)
                        str << "_int" << interleavingFactor << "_" << Z;
                    str << "[i]";
                    fout << "    ";
                    fout << strXOReq(buildWordName(E, x, y, z), strConst(str.str()));
                    fout << "; \\" << endl;
                }

                if (prepareTheta) {
                    // Prepare θ
                    if (Y == 0) {
                        fout << "    " << buildWordName(C, X, Z);
                        fout << " = ";
                        fout << buildWordName(E, X, Y, Z) << "; \\" << endl;
                    }
                    else {
                        fout << "    ";
                        fout << strXOReq(buildWordName(C, X, Z), buildWordName(E, X, Y, Z));
                        fout << "; \\" << endl;
                    }
                }
            }
        }
        fout << "\\" << endl;
    }
    fout << endl;
}

void KeccakFCodeGen::genCodeForPrepareTheta(ostream& fout, string A, string C) const
{
    for(unsigned int x=0; x<5; x++)
    for(unsigned int z=0; z<interleavingFactor; z++) {
        fout << "    " << buildWordName(C, x, z) << " = ";
        fout << 
            strXOR(buildWordName(A, x, 0, z),
            strXOR(buildWordName(A, x, 1, z),
            strXOR(buildWordName(A, x, 2, z),
            strXOR(buildWordName(A, x, 3, z), buildWordName(A, x, 4, z)))));
        fout << "; \\" << endl;
    }
    fout << endl;
}

void KeccakFCodeGen::genRoundConstants(ostream& fout) const
{
    vector<vector<LaneValue> > interleavedRC;

    for(vector<LaneValue>::const_iterator i=roundConstants.begin(); i!=roundConstants.end(); ++i) {
        vector<LaneValue> iRC(interleavingFactor, 0);
        for(unsigned int z=0; z<laneSize; z++)
            if (((*i) & ((LaneValue)1 << z)) != 0)
                iRC[z%interleavingFactor] |= ((LaneValue)1 << (z/interleavingFactor));
        interleavedRC.push_back(iRC);
    }

    for(unsigned int z=0; z<interleavingFactor; z++) {
        fout << "const UINT" << dec << wordSize << " ";
        fout << "KeccakF" << width << "RoundConstants";
        if (interleavingFactor > 1)
            fout << "_int" << interleavingFactor << "_" << z;
        fout << "[" << dec << interleavedRC.size() << "] = {" << endl;
        for(unsigned int i=0; i<interleavedRC.size(); i++) {
            if (i > 0)
                fout << "," << endl;
            fout << "    0x";
            fout.fill('0'); fout.width((wordSize+3)/4);
            fout << hex << interleavedRC[i][z];
            if (wordSize == 64)
                fout << "ULL";
            else if (wordSize == 32)
                fout << "UL";
        }
        fout << " };" << endl;
        fout << endl;
    }
}

void KeccakFCodeGen::genCopyFromStateAndXor(ostream& fout, unsigned int bitsToXor, string A, string state, string input) const
{
    unsigned int i=0;
    for(unsigned int y=0; y<5; y++)
    for(unsigned int x=0; x<5; x++)
    for(unsigned int z=0; z<interleavingFactor; z++) {
        if (outputMacros) {
            fout << "    " << buildWordName(A, x, y, z);
            fout << " = ";
            if (i*wordSize < bitsToXor)
                fout << "XOR" << dec << wordSize << "(";
            fout << "LOAD" << dec << wordSize << "(" << state << "[";
            fout.width(2); fout.fill(' '); fout << dec << i << "]";
            fout << ")";
            if (i*wordSize < bitsToXor) {
                fout << ", ";
                fout << "LOAD" << dec << wordSize << "(" << input << "[";
                fout.width(2); fout.fill(' '); fout << dec << i << "]";
                fout << ")";
                fout << ")";
            }
        }
        else {
            fout << "    " << buildWordName(A, x, y, z);
            fout << " = " << state << "[";
            fout.width(2); fout.fill(' '); fout << dec << i << "]";
            if (i*wordSize < bitsToXor) {
                fout << "^" << input << "[";
                fout.width(2); fout.fill(' '); fout << dec << i << "]";
            }
        }
        fout << "; \\" << endl;
        i++;
    }
    fout << endl;
}

void KeccakFCodeGen::genCopyToState(ostream& fout, string A, string state, string input) const
{
    unsigned int i=0;
    for(unsigned int y=0; y<5; y++)
    for(unsigned int x=0; x<5; x++)
    for(unsigned int z=0; z<interleavingFactor; z++) {
        if (outputMacros) {
            fout << "    STORE" << dec << wordSize << "(" << state << "[";
            fout.width(2); fout.fill(' '); fout << dec << i << "], ";
            fout << buildWordName(A, x, y, z);
            fout << ")";
        }
        else {
            fout << "    ";
            fout << state << "[";
            fout.width(2); fout.fill(' '); fout << dec << i << "] = ";
            fout << buildWordName(A, x, y, z);
        }
        fout << "; \\" << endl;
        i++;
    }
    fout << endl;
}

void KeccakFCodeGen::genCopyStateVariables(ostream& fout, string X, string Y) const
{
    for(unsigned int y=0; y<5; y++)
    for(unsigned int x=0; x<5; x++)
    for(unsigned int z=0; z<interleavingFactor; z++) {
        fout << "    ";
        fout << buildWordName(X, x, y, z);
        fout << " = ";
        fout << buildWordName(Y, x, y, z);
        fout << "; \\" << endl;
    }
    fout << endl;
}

void KeccakFCodeGen::genMacroFile(ostream& fout, bool laneComplementing) const
{
    fout << "/*" << endl;
    fout << "Code automatically generated by KeccakTools!" << endl;
    fout << endl;
    fout << "The Keccak sponge function, designed by Guido Bertoni, Joan Daemen," << endl;
    fout << "Micha\xC3\xABl Peeters and Gilles Van Assche. For more information, feedback or" << endl;
    fout << "questions, please refer to our website: http://keccak.noekeon.org/" << endl;
    fout << endl;
    fout << "Implementation by the designers," << endl;
    fout << "hereby denoted as \"the implementer\"." << endl;
    fout << endl;
    fout << "To the extent possible under law, the implementer has waived all copyright" << endl;
    fout << "and related or neighboring rights to the source code in this file." << endl;
    fout << "http://creativecommons.org/publicdomain/zero/1.0/" << endl;
    fout << "*/" << endl;
    fout << endl;
    fout << "#define declareABCDE \\" << endl;
    genDeclarations(fout);
    fout << "#define prepareTheta \\" << endl;
    genCodeForPrepareTheta(fout);
    if (laneComplementing) {
        const SliceValue inChiMask = 0x9d14ad;
        const SliceValue outChiMask = 0x121106; // see Keccak main document on lane complementing
        fout << "#ifdef UseBebigokimisa" << endl;
        genCodeForRound(fout, true, inChiMask, outChiMask,
            "A##", "B", "C", "D", "E##", 
            "#define thetaRhoPiChiIotaPrepareTheta(i, A, E) \\");
        genCodeForRound(fout, false, inChiMask, outChiMask,
            "A##", "B", "C", "D", "E##", 
            "#define thetaRhoPiChiIota(i, A, E) \\");
        fout << "#else // UseBebigokimisa" << endl;
    }
    genCodeForRound(fout, true, 0, 0,
        "A##", "B", "C", "D", "E##", 
        "#define thetaRhoPiChiIotaPrepareTheta(i, A, E) \\");
    genCodeForRound(fout, false, 0, 0,
        "A##", "B", "C", "D", "E##", 
        "#define thetaRhoPiChiIota(i, A, E) \\");
    if (laneComplementing)
        fout << "#endif // UseBebigokimisa" << endl << endl;
    genRoundConstants(fout);
    if (width == 1600) {
        fout << "#define copyFromStateAndXor576bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 576);
        fout << "#define copyFromStateAndXor832bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 832);
        fout << "#define copyFromStateAndXor1024bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 1024);
        fout << "#define copyFromStateAndXor1088bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 1088);
        fout << "#define copyFromStateAndXor1152bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 1152);
        fout << "#define copyFromStateAndXor1344bits(X, state, input) \\" << endl;
        genCopyFromStateAndXor(fout, 1344);
    }
    fout << "#define copyFromState(X, state) \\" << endl;
    genCopyFromStateAndXor(fout, 0);
    fout << "#define copyToState(state, X) \\" << endl;
    genCopyToState(fout);
    fout << "#define copyStateVariables(X, Y) \\" << endl;
    genCopyStateVariables(fout);
}

string KeccakFCodeGen::strROL(const string& symbol, unsigned int amount) const
{
    stringstream str;

    if (amount > 0)
        str << "ROL" << dec << wordSize << "(";
    str << symbol;
    if (amount > 0) {
        str << ", " << dec;
        str << amount << ")";
    }
    return str.str();
}

string KeccakFCodeGen::strXOR(const string& A, const string& B) const
{
    stringstream str;

    if (outputMacros) {
        str << "XOR" << dec << wordSize << "(";
        str << A << ", " << B << ")";
    }
    else
        str << A << "^" << B;
    return str.str();
}

string KeccakFCodeGen::strXOReq(const string& A, const string& B) const
{
    stringstream str;

    if (outputMacros) {
        str << "XOReq" << dec << wordSize << "(";
        str << A << ", " << B << ")";
    }
    else
        str << A << " ^= " << B;
    return str.str();
}

string KeccakFCodeGen::strANDORnot(const string& A, const string& B, bool LC1, bool LC2, bool LOR) const
{
    stringstream str;

    if (outputMacros) {
        str << (LOR ? "OR" : "AND") << (LC1 ? "n" : "u") << (LC2 ? "n" : "u") 
            << dec << wordSize << "(";
        str << A << ", " << B;
        str << ")";
    }
    else {
        str << "(";
        str << (LC1 ? "(~" : "  ") << A << (LC1 ? ")" : " ");
        str << (LOR ? "|" : "&");
        str << (LC2 ? "(~" : "  ") << B << (LC2 ? ")" : " ");
        str << ")";
    }
    return str.str();
}

string KeccakFCodeGen::strNOT(const string& A, bool complement) const
{
    stringstream str;

    if (outputMacros) {
        if (complement)
            str << "NOT" << dec << wordSize << "(";
        str << A;
        if (complement)
            str << ")";
    }
    else {
        if (complement)
            str << "(~";
        else
            str << "  ";
        str << A;
        if (complement)
            str << ")";
        else
            str << " ";
    }
    return str.str();
}

string KeccakFCodeGen::strConst(const string& A) const
{
    if (outputMacros) {
        stringstream str;

        str << "CONST" << dec << wordSize << "(";
        str << A;
        str << ")";
        return str.str();
    }
    else
        return A;
}

string KeccakFCodeGen::getName() const
{
    stringstream a;
    a << "KeccakF-" << dec << width;
    return a.str();
}
