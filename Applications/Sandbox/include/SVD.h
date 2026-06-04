#pragma once 
#include "Mat4d.h" 
#include <array> 

namespace NkMath { 

    // SVDResult : résultat de la décomposition A = U * Σ * Vᵀ 
    struct SVD3x3 { 
        Mat3d U;      // m×m orthogonale 
        Vec3d sigma;  // valeurs singulières triées décroissantes 
        Mat3d V;      // n×n orthogonale (attention : V, pas Vᵀ) 
    
        // Vérification : ||A - U*Σ*Vᵀ|| < kEps 
        double residual(const Mat3d& A) const {
            Mat3d S{};
            S(0,0) = sigma.x;
            S(1,1) = sigma.y;
            S(2,2) = sigma.z;

            Mat3d R = U * S * V.Transposed();

            double err = 0.0;
            for(int i=0;i<3;i++)
                for(int j=0;j<3;j++){
                    double d = R(i,j) - A(i,j);
                    err += d*d;
                }
            return std::sqrt(err);
        }

    
        // Pseudo-inverse A⁺ = V * Σ⁺ * Uᵀ 
        Mat3d pseudoInverse(double threshold = 1e-10) const {
            Mat3d Splus{};
            if(sigma.x > threshold) 
                Splus(0,0)=1.0/sigma.x;

            if(sigma.y > threshold) 
                Splus(1,1)=1.0/sigma.y;

            if(sigma.z > threshold) 
                Splus(2,2)=1.0/sigma.z;

            return V * Splus * U.Transposed();
        }
    
        // Solution de Ax=0 : dernière colonne de V 
        Vec3d nullSpaceVector() const {
            // plus petite valeur singulière = dernière colonne
            return V.col(2);
        }
    
        // Décomposition polaire A = R*S : R = U*Vᵀ 
        Mat3d rotationPart() const {
            Mat3d R = U * V.Transposed();

            if(R.Det() < 0){
                Mat3d Ufix = U;
                
                for(int i=0;i<3;i++) 
                    Ufix(i,2) *= -1;

                R = Ufix * V.Transposed();
            }
            return R;
        }
    }; 
    
    // Jacobi eigen decomposition (sym 3x3)
    inline void eigenSymmetric3x3(const Mat3d& A, Mat3d& V, Vec3d& D) {
        Mat3d M = A;
        V = Mat3d::Identity();

        for(int iter=0; iter<10; iter++){
            // trouver plus grand élément hors-diagonal
            int p=0,q=1;
            double max = std::fabs(M(0,1));

            if(std::fabs(M(0,2)) > max){ p=0;q=2; max=std::fabs(M(0,2)); }
            if(std::fabs(M(1,2)) > max){ p=1;q=2; max=std::fabs(M(1,2)); }

            if(max < 1e-12) break;

            double theta = 0.5 * (M(q,q)-M(p,p)) / M(p,q);
            double t = (theta >= 0)
                ? 1.0/(theta + std::sqrt(theta*theta+1))
                : -1.0/(-theta + std::sqrt(theta*theta+1));

            double c = 1.0 / std::sqrt(t*t+1);
            double s = t * c;

            // rotation
            for(int k=0;k<3;k++){
                double mkp = M(k,p);
                double mkq = M(k,q);
                M(k,p) = c*mkp - s*mkq;
                M(k,q) = s*mkp + c*mkq;
            }

            for(int k=0;k<3;k++){
                double mpk = M(p,k);
                double mqk = M(q,k);
                M(p,k) = c*mpk - s*mqk;
                M(q,k) = s*mpk + c*mqk;
            }

            // accumuler vecteurs propres
            for(int k=0;k<3;k++){
                double vkp = V(k,p);
                double vkq = V(k,q);
                V(k,p) = c*vkp - s*vkq;
                V(k,q) = s*vkp + c*vkq;
            }
        }

        D = { M(0,0), M(1,1), M(2,2) };

        // tri décroissant
        for(int i=0;i<3;i++){
            for(int j=i+1;j<3;j++){
                if(D[j] > D[i]){
                    std::swap(D[i], D[j]);
                    for(int k=0;k<3;k++)
                        std::swap(V(k,i), V(k,j));
                }
            }
        }
    }

    // SVD 3x3
    // Fonction principale
    inline SVD3x3 svd3x3(const Mat3d& A) {
        Mat3d AtA = A.Transposed() * A;

        Mat3d V;
        Vec3d D;

        eigenSymmetric3x3(AtA, V, D);

        Vec3d sigma{
            std::sqrt(std::max(0.0, D.x)),
            std::sqrt(std::max(0.0, D.y)),
            std::sqrt(std::max(0.0, D.z))
        };

        Mat3d U;

        for(int i=0;i<3;i++){
            if(sigma[i] > kEps){
                Vec3d vi = V.col(i);
                Vec3d ui = (A * vi) * (1.0 / sigma[i]);
                U.setCol(i, ui.Normalized());
            }
        }

        return {U, sigma, V};
    }

    // Polar decomposition (rotation)
    inline Mat3d polarRotation(const Mat3d& A){
        SVD3x3 s = svd3x3(A);
        return s.rotationPart();
    }
}
