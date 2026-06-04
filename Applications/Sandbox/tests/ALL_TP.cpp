#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <iostream>
#include <random>
#include <cstdlib>

#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "SVD.h"
#include "Quat.h" 
#include "IntegralImage.h" 


using namespace NkMath;

const int width = 512, height = 512;
NkImage img(width, height);

std::mt19937 rng(42);
std::uniform_real_distribution<double> dist(-10.0, 10.0);

std::vector<Vec4d> cube = {
    {-0.5,-0.5,-0.5,1}, {0.5,-0.5,-0.5,1},
    {0.5, 0.5,-0.5,1}, {-0.5, 0.5,-0.5,1},
    {-0.5,-0.5, 0.5,1}, {0.5,-0.5, 0.5,1},
    {0.5, 0.5, 0.5,1}, {-0.5, 0.5, 0.5,1}
};
    
// Arêtes du cube (12)
std::vector<Vec2d> edges = {
    {0,1},{1,2},{2,3},{3,0}, // face arrière
    {4,5},{5,6},{6,7},{7,4}, // face avant
    {0,4},{1,5},{2,6},{3,7}  // connexions
};

// Matrices de Vue et Projection pour le rasteriseur logiciel
Vec3d eye{0,1,3}, target{0,0,0}, up{0,1,0};
Mat4d V = LookAt(eye, target, up);
Mat4d P = Perspective(60.0, double(width)/height, 0.1, 100.0);

// Semaine 1

// TP1 : InspectFloat(float x)

TEST_CASE(TP1, Fonction_InspectFloat) {
    inspectFloat(0.1f);
    inspectFloat(1.0f);
    inspectFloat(1.0f / 0.0f);
    inspectFloat(std::sqrt(-1.0f));
    inspectFloat(-0.0f);
    inspectFloat(0.0f);
    inspectFloat(std::numeric_limits<float>::min());
}

 // TP2 : Kahan et Welford

TEST_CASE(TP2, Kahan_Et_Welford) {
    float s1, s2;
    std::vector<float> v;

    // 1. Tableau de 1 000 000  de copies de 0.1f
    std::vector<float> data(1'000'000, 0.1f);
    
    // 2. Comparaison : Somme accumulate vs Somme Kahan
    s1 = std::accumulate(data.begin(), data.end(), 0.0f);
    s2 = kahanSum(data);
    logger.Info("\nSum with accumulate : {0}\nKahan sum : {1}\nReal value : 100000.0", s1, s2);
    
    // 3. Calcul de variance : Variance naïve vs Variance Welford
    v = std::vector<float>({1e8f, 1e8f, 1.0f, 2.0f});
    logger.Info("\nVariance Naive   : {0}\nVariance de Welford : {1}", varianceNaive(v), varianceWelford(v));

    // 4. Comparaison : Epsilon machine par boucle vs std::numeric_limits<float>::epsilon() 
    logger.Info("\nEpsilon Machine (loop) : {0}\nEpsilon Machine (std)  : {1}", epsilonMachine(), std::numeric_limits<float>::epsilon());
}


// TP3 : NkMath/Float.h

TEST_CASE(TP3, Tests_Sur_Floath) {
    // 1. Tests isFiniteValid    
    ASSERT_TRUE(!isFiniteValid(std::numeric_limits<float>::quiet_NaN())); // 1
    ASSERT_TRUE(!isFiniteValid(std::numeric_limits<float>::infinity()));  // 2
    ASSERT_TRUE(!isFiniteValid(-std::numeric_limits<float>::infinity())); // 3
    ASSERT_TRUE(isFiniteValid(0.0f));                                     // 4
    ASSERT_TRUE(isFiniteValid(1.0f));                                     // 5

    // 2. Tests nearlyZero 
    ASSERT_TRUE(nearlyZero(0.0f, 1e-6f));     // 6
    ASSERT_TRUE(!nearlyZero(1e-5f, 1e-6f));   // 7
    ASSERT_TRUE(nearlyZero(1e-7f, 1e-6f));    // 8

    ASSERT_TRUE(nearlyZero(-1e-7f, 1e-6f));   // 9
    ASSERT_TRUE(!nearlyZero(-1e-6f, 1e-7f));  // 10

    ASSERT_TRUE(nearlyZero(1e-3f, 1e-2f));    // 11
    ASSERT_TRUE(!nearlyZero(1e-2f, 1e-3f));   // 12

    ASSERT_TRUE(nearlyZero(5e-8f, 1e-7f));    // 13

    // 3. Tests approxEq
    ASSERT_TRUE(approxEq(1.0f, 1.0f, 1e-6f));           // 14
    ASSERT_TRUE(approxEq(1.0f, 1.0000001f, 1e-5f));     // 15
    ASSERT_TRUE(!approxEq(1.0f, 1.1f, 1e-3f));          // 16

    ASSERT_TRUE(approxEq(0.0f, 1e-7f, 1e-6f));          // 17
    ASSERT_TRUE(!approxEq(0.0f, 1e-4f, 1e-6f));         // 18

    ASSERT_TRUE(approxEq(-1.0f, -1.000001f, 1e-5f));    // 19
    ASSERT_TRUE(!approxEq(-1.0f, -1.1f, 1e-2f));        // 20

    ASSERT_TRUE(approxEq(1000.0f, 1000.0001f, 1e-3f));  // 21
    ASSERT_TRUE(approxEq(1000.0f, 1001.0f, 1e-3f));     // 22

    ASSERT_TRUE(approxEq(1e-7f, 2e-7f, 1e-6f));         // 23

    // 4. Tests kahanSum vs accumulate  
    float s1, s2;
    std::vector<float> v;
  
    v = std::vector<float>(1000, 0.1f);
    s1 = std::accumulate(v.begin(), v.end(), 0.0f);
    s2 = kahanSum(v);
    ASSERT_TRUE(std::fabs(s2 - 100.0f) < std::fabs(s1 - 100.0f)); // 24
    
    v = std::vector<float>(10000, 0.1f);
    s1 = std::accumulate(v.begin(), v.end(), 0.0f);
    s2 = kahanSum(v);
    ASSERT_TRUE(std::fabs(s2 - 1000.0f) < std::fabs(s1 - 1000.0f)); // 25
    
    v = std::vector<float>({1e8f, 1.0f, -1e8f});
    s1 = std::accumulate(v.begin(), v.end(), 0.0f);
    s2 = kahanSum(v);
    ASSERT_TRUE(std::fabs(s2 - 1.0f) <= std::fabs(s1 - 1.0f)); // 26

    v = std::vector<float>({1.0f, 1e8f, -1e8f});
    s1 = std::accumulate(v.begin(), v.end(), 0.0f);
    s2 = kahanSum(v);
    ASSERT_TRUE(std::fabs(s2 - 1.0f) <= std::fabs(s1 - 1.0f)); // 27
    
    v = std::vector<float>(100000, 0.01f);
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 1000.0f, 1e-2f)); // 28

    v = std::vector<float>(100000, 1e-5f);
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 1.0f, 1e-3f)); // 29
    
    v = std::vector<float>({0.1f, 0.2f, 0.3f});
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 0.6f, 1e-6f)); // 30

    v = std::vector<float>(50000, 0.2f);
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 10000.0f, 1e-2f)); // 31
    
    v = std::vector<float>({1e7f, 1.0f, 1.0f, -1e7f});
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 2.0f, 1e-3f)); // 32

    v = std::vector<float>(1000000, 0.1f);
    s2 = kahanSum(v);
    ASSERT_TRUE(approxEq(s2, 100000.0f, 1e-1f)); // 33
}

// Semaine 2

// TP1 : Vec2d complet et implémentations

TEST_CASE(TP1, Vec2d_Et_Implementation) {
    // 1. Dot product
    ASSERT_TRUE(Dot({1,0}, {0,1}) == 0.0);      // 1
    ASSERT_TRUE(Dot({1,0}, {1,0}) == 1.0);      // 2
    ASSERT_TRUE(Dot({3,4}, {3,4}) == 25.0);     // 3
    ASSERT_TRUE(Dot({-1,0}, {1,0}) == -1.0);    // 4
    ASSERT_TRUE(Dot({2,3}, {4,5}) == 23.0);     // 5
    ASSERT_TRUE(Dot({0,0}, {5,7}) == 0.0);      // 6
    
    // 2. CROSS2D 
    ASSERT_TRUE(Cross2D({1,0}, {0,1}) == 1.0);   // 7
    ASSERT_TRUE(Cross2D({0,1}, {1,0}) == -1.0);  // 8
    ASSERT_TRUE(Cross2D({1,1}, {1,1}) == 0.0);   // 9
    ASSERT_TRUE(Cross2D({2,0}, {0,2}) == 4.0);   // 10
    
    // 3. NORMALISATION
    Vec2d w = {3,4};
    Vec2d n = w.Normalized();
    ASSERT_TRUE(std::fabs(n.Norm() - 1.0) < kEps);   // 11
    
    // direction conservée
    ASSERT_TRUE(std::fabs(n.x - 0.6) < kEps);    // 12
    ASSERT_TRUE(std::fabs(n.y - 0.8) < kEps);    // 13
    
    // vecteur unitaire reste inchangé
    Vec2d u = {1,0};
    u = u.Normalized();
    ASSERT_TRUE(std::fabs(u.x - 1.0) < kEps);   // 14
    
    // 4. OPERATOR []
    w = {10, 20};
    ASSERT_TRUE(w[0] == 10.0);   // 15
    ASSERT_TRUE(w[1] == 20.0);   // 16

    w[0] = 30;
    ASSERT_TRUE(w.x == 30.0);    // 17

    w[1] = 40;
    ASSERT_TRUE(w.y == 40.0);    // 18
    
    u = {5, 6};
    ASSERT_TRUE(u[0] == 5.0);    // 19
    
    // 5. STATIC ASSERT 
    static_assert(sizeof(Vec2d) == 16, "Vec2d must be 16 bytes"); // 20
}

// TP2 : Vec3d avec Gram-Schmidt

TEST_CASE(TP2, Vec3d_Et_GramSchmidt) {
    // 1 & 2. Cross Product
    Vec3d i = {1,0,0}, j = {0,1,0}, k = {0,0,1};

    // règle main droite
    ASSERT_TRUE(ApproxVec(Cross(i, j), k));               // 1
    ASSERT_TRUE(ApproxVec(Cross(j, i), {0,0,-1}));        // 2
    // base complète
    ASSERT_TRUE(ApproxVec(Cross(j, k), i));               // 3
    ASSERT_TRUE(ApproxVec(Cross(k, i), j));               // 4
    // orthogonalité
    ASSERT_TRUE(approxEq(Dot(Cross(i, j), i), 0));        // 5
    ASSERT_TRUE(approxEq(Dot(Cross(i, j), j), 0));        // 6
    
    // 2. Gram-Schmidt sur 10 triplets aléatoires     
    for(int t = 0; t < 10; ++t) {
        Vec3d a{dist(rng), dist(rng), dist(rng)};
        Vec3d b{dist(rng), dist(rng), dist(rng)};
        Vec3d c{dist(rng), dist(rng), dist(rng)};
    
        // Gram-Schmidt
        Vec3d ui = a.Normalized();
        Vec3d vi = (b - Project(b, ui)).Normalized();
        Vec3d wi = (c - Project(c, ui) - Project(c, vi)).Normalized();
        // normes
        ASSERT_TRUE(approxEq(ui.Norm(), 1.0));  // 7
        ASSERT_TRUE(approxEq(vi.Norm(), 1.0));  // 8
        ASSERT_TRUE(approxEq(wi.Norm(), 1.0));  // 9
    
        // orthogonalité
        ASSERT_TRUE(approxEq(Dot(ui, vi), 0.0));  // 10
        ASSERT_TRUE(approxEq(Dot(ui, wi), 0.0));  // 11
        ASSERT_TRUE(approxEq(Dot(vi, wi), 0.0));  // 12
    }
    
    // 3. Project et Reject
    i = {3,4,0}, j = {1,0,0};
    Vec3d proj = Project(i, j);
    Vec3d rej = Reject(i, j);
    ASSERT_TRUE(ApproxVec(proj + rej, i)); // 13
}

//  TP3: Vec4d et projection perspective simple

TEST_CASE(TP3, Vec4d_Et_PPS) {
    std::vector<Vec2d> proj; 
    
    // Position de la camera et projections
    double z_cam = 2.0;
    for(auto& p : cube){
        p.z += z_cam;
        proj.push_back(ProjectPoint(p));
    }
    
    // Dessin des coins dans Image
    for(const auto& p : proj) {
        int x = (int)p.x, y = (int)p.y;
        // petit carré 
        for(int dx = -2; dx <= 2; dx++)
            for(int dy = -2; dy <= 2; dy++)
                img.SetPixel(x+dx, y+dy, 255, 0, 0);
    }
    
    // Dessin dans l'image
    for(auto edge : edges)
        img.DrawLine((int)proj[edge.x].x, (int)proj[edge.x].y, (int)proj[edge.y].x, (int)proj[edge.y].y);
    img.SavePPM("cube.ppm");
}

// Semaine 3

// TP1 : Mat4d et inverse

TEST_CASE(TP1, Mat4d_Et_Inverse) {
    Mat4d m, r, inv;

    for(int t=0;t<10;t++) {
        for(int i=0;i<4;i++)
            for(int j=0;j<4;j++)
                m(i, j) = dist(rng);
        
        // 1. M × Identity() == M
        r = m * Mat4d::Identity();
        ASSERT_TRUE(ApproxMat(r, m)); // 1–10
    
        // 2. M × M⁻¹ == Identity()
        if(Inverse(m, inv)) // skip singulière
            ASSERT_TRUE(ApproxMat(m * inv, Mat4d::Identity(), 1e-10f)); // 11–20
    }
    
    // 3. Inverse d'une matrice singulière 
    m = Mat4d::Identity();
    // rendre singulière (ligne dupliquée)
    for(int j=0;j<4;j++)
        m(1, j) = m(0, j);
    ASSERT_TRUE(!Inverse(m, inv)); // 21
    
    // 4. RotateAxis({0,1,0}, PI/2) × {1,0,0,1} == {0,0,-1,1} 
    r = Mat4d::RotateAxis({0,1,0}, NKENTSEU_PI_DOUBLE / 2.0f);
    Vec4d s = {1,0,0,1}, q = r * s;

    ASSERT_TRUE(approxEq(q.x, 0.0));   // 22
    ASSERT_TRUE(approxEq(q.y, 0.0));   // 23
    ASSERT_TRUE(approxEq(q.z, -1.0));  // 24
}

// TP2 : Rasteriseur logiciel et rotation du cube

TEST_CASE(TP2, RotationCube) {
    for(int frame = 0; frame < 10; frame++){
        img = NkImage(width, height);
        double angle = frame * 0.3;
        Mat4d R = Mat4d::RotateAxis(up, angle);
        std::vector<Vec3d> screen;
    
        for(auto v : cube){
            Vec4d p = P * (V * (R * v));
            screen.push_back(ProjectToScreen(p, width, height));
        }
    
        for(auto edge : edges)
            img.DrawLine((int)screen[edge.x].x, (int)screen[edge.x].y, (int)screen[edge.y].x, (int)screen[edge.y].y, 255);
        img.SavePPM("frame_TP8_"+std::to_string(frame)+".ppm");
    }
}


// TP3 : TRS et Décomposition

TEST_CASE(TP3, TRS_Et_Decomposition) {
    dist = std::uniform_real_distribution<double>(-5.0, 5.0);

    for(int t = 0; t < 20; t++){
        Vec3d outT{dist(rng), dist(rng), dist(rng)};
        Vec3d outR{dist(rng), dist(rng), dist(rng)};
        Vec3d outS{dist(rng) + 6, dist(rng) + 6, dist(rng) + 6}; // éviter <= 0
        
        // 1. Construction de TRS
        Mat4d M = TRS(outT, outR, outS);
        
        // 2. Décomposition de  TRS
        Vec3d T2, R2, S2;
        DecomposeTRS(M, T2, R2, S2);
    
        // 3. Vérification des valeurs
        ASSERT_TRUE(ApproxVec(outT, T2));
        ASSERT_TRUE(ApproxVec(outS, S2));

        // Rotation : tolérance plus large (ambiguïtés angles)
        ASSERT_TRUE(ApproxVec(outR, R2, 5.0));
    }
}


// TPs semaine 4

// TP1 : Quaternions complets 

TEST_CASE(TP1, Quaternions) {
    Mat3d m1, m2, m3;
    Quat q1, q2, q3;

    // 1. Vérifier Rotate et FromAxis (avec Pi)
    Vec3d i = {1,0,0};
    q1 = FromAxisAngle({0,1,0}, NKENTSEU_PI_DOUBLE / 2.0f);
    Vec3d j = Rotate(q1, i);

    ASSERT_TRUE(std::fabs(j.x - 0.0) < kEps);
    ASSERT_TRUE(std::fabs(j.y - 0.0) < kEps);
    ASSERT_TRUE(std::fabs(j.z + 1.0) < kEps);
    
    // 2. Aller-retour Quat => Mat3d => Quat
    dist = std::uniform_real_distribution<double>(-1.0, 1.0);

    for(int t = 0; t < 50; t++){
        q1 = { dist(rng), dist(rng), dist(rng), dist(rng) };
        q1 = q1.Normalized();
    
        m1 = ToMat3(q1);
        q2 = FromMat3(m1);
        q2 = q2.Normalized();
    
        ASSERT_TRUE(ApproxQuat(q1, q2, 1e-4f));
    }
    
    // 3.  Vérification: Quat x Quat.Inverse() = identité
    for(int t = 0; t < 50; t++){
        q1 = { dist(rng), dist(rng), dist(rng), dist(rng) };
        q1 = q1.Normalized();
        q2 = q1.Inverse();
        q3 = q1 * q2;
        ASSERT_TRUE(ApproxQuat(q3, Quat::Identity(), 1e-4f));
    }
}


// TP2 : Animation SLERP

TEST_CASE(TP2, Animation_SLERP) {
    Quat q1, q2, q3;
    
    // 1. Animation d'une rotation sur 60 frames via le rasteriseur
    q1 = FromAxisAngle({0,1,0}, 0);
    q2 = FromAxisAngle({0,1,0}, NKENTSEU_PI_DOUBLE);
    
    for(int frame = 0; frame < 60; frame++){
        double t = frame / 59.0;
        Quat q = Slerp(q1, q2, t);
        Mat4d R = FromRT(ToMat3(q), {0,0,0});
    
        img = NkImage(width, height);
        std::vector<Vec3d> screen;
        for(auto v : cube){
            Vec4d p = P * (V * (R * v));     // rotation + Vue + Projection
            screen.push_back(ProjectToScreen(p, width, height));
        }
    
        for(auto edge : edges)
            img.DrawLine((int)screen[edge.x].x, (int)screen[edge.x].y, (int)screen[edge.y].x, (int)screen[edge.y].y, 255);
        img.SavePPM("Slerp__frame_TP11_"+std::to_string(frame)+".ppm");
    }
    
    for(int frame = 0; frame < 60; frame++){
        double t = frame / 59.0;
        Quat q = Lerp(q1, q2, t);
        Mat4d R = FromRT(ToMat3(q), {0,0,0});
    
        img = NkImage(width, height);
        std::vector<Vec3d> screen;
        for(auto v : cube){
            Vec4d p = P * (V * (R * v));     // rotation + Vue + Projection
            screen.push_back(ProjectToScreen(p, width, height));
        }
    
        for(auto edge : edges)
            img.DrawLine((int)screen[edge.x].x, (int)screen[edge.x].y, (int)screen[edge.y].x, (int)screen[edge.y].y, 255);
        img.SavePPM("Lerp__frame_TP11_"+std::to_string(frame)+".ppm");
    }
}


// Semaine 5

// TP1 : Test de l'implémentation de SVD fournie

TEST_CASE(TP1, TestsSVD) {
    Mat3d A{}, Aplus{}, S{}, R{}, Iu{}, Iv{};
    SVD3x3 s;
    Vec3d b, x, r;

    // 1. Tests svd3x3 sur 20 matrices aléatoires
    dist = std::uniform_real_distribution<double>(-1.0, 1.0);

    for(int i = 0; i < 20; i++){
        for(int j = 0; j < 3; j++)
            for(int k = 0; k < 3; k++)
                A(j, k) = dist(rng);

        s = svd3x3(A);

        // Reconstruction
        S(0,0) = s.sigma.x;
        S(1,1) = s.sigma.y;
        S(2,2) = s.sigma.z;

        R = s.U * S * s.V.Transposed();

        ASSERT_TRUE((R - A).norm() < 1e-6);

        // Orthogonalité U, V
        Iu = s.U * s.U.Transposed();
        Iv = s.V * s.V.Transposed();

        ASSERT_TRUE((Iu - Mat3d::Identity()).norm() < 1e-6);
        ASSERT_TRUE((Iv - Mat3d::Identity()).norm() < 1e-6);

        // Sigma triés
        ASSERT_TRUE(s.sigma.x >= s.sigma.y);
        ASSERT_TRUE(s.sigma.y >= s.sigma.z);
    }

    // 2. Tests sur une matrice rang-déficiente : les dernières σ doivent être ≈ 0 
    // rang 1 : lignes dépendantes
    A.setRow(0, {1,2,3});
    A.setRow(1, {2,4,6});
    A.setRow(2, {3,6,9});

    s = svd3x3(A);
    ASSERT_TRUE(std::fabs(s.sigma.z) < 1e-6);

    // 3. Vérification pseudo-inverse (3×2)
    // système sur-déterminé Ax = b
    A.setRow(0, {1, 2, 0});        
    A.setRow(1, {3, 4, 0});
    A.setRow(2, {5, 6, 0});
    b = Vec3d(7, 8, 9);
    s = svd3x3(A);
    Aplus = s.pseudoInverse();
    x = Aplus * b;
    
    // vérifier Ax ≈ b (moindres carrés)
    r = A * x - b;
    ASSERT_TRUE(r.Norm() < 1e-5);
}


// TP2 : Résolution d'homographie

TEST_CASE(TP2, ResolutionHomographie) {
    (void)0; // placeholder pour éviter warning "unused function"
}


// Semaine 6

// TP1 : NkImage de base

TEST_CASE(TP1, NkImageDeBase) {
    // 2. Créer une image 512x512 avec un dégradé et des formes
    // dégradé
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            uint8_t r = (uint8_t)(255.0 * x / width);
            uint8_t g = (uint8_t)(255.0 * y / height);
            uint8_t b = 128;

            img.SetPixelRGBA(x,y,r,g,b);
        }
    }

    // rectangle
    for(int y = 100; y < 200; y++)
        for(int x = 100; x < 300; x++)
            img.SetPixelRGBA(x,y,255,0,0);

    // ligne diagonale
    for(int i = 0; i < 512; i++)
        img.SetPixelRGBA(i,i,0,255,0);

    // cercle simple
    int cx=256, cy=256, r=80;
    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            int dx = x - cx, dy = y - cy;
            if(dx * dx + dy * dy < r * r)
                img.SetPixelRGBA(x,y,0,0,255);
        }
    }

    // Sauvegarde
    img.SavePPM("fade_test_image.ppm");
}


// TP2 : SampleBilinear et Convolve

TEST_CASE(TP2, SampleBilinearEtConvolve) {
    // 58. Appliquez flou Gauss 5×5 et Sobel horizontal/vertical sur une photo PPM
    std::vector<double> gaussian5 = {
        1, 4, 6, 4, 1,
        4,16,24,16, 4,
        6,24,36,24, 6,
        4,16,24,16, 4,
        1, 4, 6, 4, 1
    };

    for(auto& v : gaussian5) v /= 256.0;

    std::vector<double> sobelX = {
        -1,0,1,
        -2,0,2,
        -1,0,1
    };

    std::vector<double> sobelY = {
        -1,-2,-1,
        0, 0, 0,
        1, 2, 1
    };

    img.LoadPPM("input_arUCO.ppm");

    auto t0 = std::chrono::high_resolution_clock::now();

    // 1. flou
    NkImage blurred = img.Convolve(gaussian5, 5);

    // 2. gradients
    NkImage gx = blurred.Convolve(sobelX, 3);
    NkImage gy = blurred.Convolve(sobelY, 3);

    // 3. magnitude
    NkImage edges = NkImage::CombineGradient(gx, gy);

    // 4. sauvegarde
    edges.SavePPM("convolve_test_image.ppm");

    // 5. Mesurer le temps d'exécution de la convolution pour 5×5 et 3×3
    auto t1 = std::chrono::high_resolution_clock::now();

    logger.Info("Convolution 5x5 + 3x3 + magnitude took {0} ms", std::chrono::duration<double, std::milli>(t1-t0).count());
}


// TP3 : Image intégrale et seuillage

TEST_CASE(TP3, ImageIntegraleEtSeuillage) {
    // 62. AR UCO : détectez les coins via l'image intégrale + seuillage
    NkImage img;
    img.LoadPPM("input_arUCO.ppm");

    NkImage bin = AdaptiveThreshold(img, 31, 7);
    bin.SavePPM("adaptivethreshold_test_image.ppm");

    // 63. Temps de construction de l'image intégrale
    auto t0 = std::chrono::high_resolution_clock::now();

    IntegralImage ii(img.ToGrayscale(), img.Width(), img.Height());

    auto t1 = std::chrono::high_resolution_clock::now();

    logger.Info("Integral image construction took {0} ms", std::chrono::duration<double, std::milli>(t1-t0).count());
}