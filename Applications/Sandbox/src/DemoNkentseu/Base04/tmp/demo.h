// Déclaration dans la classe
NkGrid3D mGrid3D;

// Initialisation
if (!mGrid3D.Init(device)) {
    PushLog("Erreur grille 3D");
}

// Configuration
NkGrid3DConfig gridCfg;
gridCfg.baseUnit = 2.0f;
gridCfg.subDivisions = 4;
gridCfg.infinite = true;
gridCfg.fadeStart = 30.f;
gridCfg.fadeEnd = 80.f;
gridCfg.showAxes = true;
gridCfg.showSolidFloor = false;
mGrid3D.SetConfig(gridCfg);

// Dans la boucle de rendu, après avoir bindé le depth buffer en texture :
mGrid3D.Draw(cmd, viewMatrix, projMatrix, depthTexture, width, height);