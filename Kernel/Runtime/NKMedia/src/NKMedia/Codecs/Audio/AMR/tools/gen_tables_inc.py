# ============================================================================
# Generation de NkAmrTables.inc a partir des donnees extraites mecaniquement :
#  - tabs_dump.json   : tableaux des fichiers *.tab de 3GPP TS 26.073 (donnees)
#  - order_tables.txt : tables B.1..B.8 de 3GPP TS 26.101 Annexe B (docx)
# Verifie au passage que sort_* (26.073) == Annexe B (26.101).
# ============================================================================
import json, sys

arrays = json.load(open("tabs_dump.json"))
by_key = {}
for a in arrays:
    by_key[(a["file"], a["name"])] = a["data"]

# --- tables Annexe B (26.101) ---
order = {}
lines = open("order_tables.txt").read().strip().split("\n")
for i in range(0, len(lines), 2):
    order[lines[i]] = [int(x) for x in lines[i + 1].split(",")]

sort_names = {"B1": "sort_475", "B2": "sort_515", "B3": "sort_59", "B4": "sort_67",
              "B5": "sort_74", "B6": "sort_795", "B7": "sort_102", "B8": "sort_122"}
for b, sn in sort_names.items():
    ref = by_key[("bitno.tab", sn)]
    if ref != order[b]:
        print("!! divergence %s vs %s" % (b, sn))
        sys.exit(1)
print("Recoupement 26.101 Annexe B == 26.073 sort_* : OK (8/8)")

# --- selection des tables utiles au DECODEUR ---
# (nom C genere, fichier source, nom source, type)
WANT = [
    # ordonnancement des bits (26.101 Annexe B == bitno.tab sort_*)
    ("kOrder475", "bitno.tab", "sort_475"),
    ("kOrder515", "bitno.tab", "sort_515"),
    ("kOrder59",  "bitno.tab", "sort_59"),
    ("kOrder67",  "bitno.tab", "sort_67"),
    ("kOrder74",  "bitno.tab", "sort_74"),
    ("kOrder795", "bitno.tab", "sort_795"),
    ("kOrder102", "bitno.tab", "sort_102"),
    ("kOrder122", "bitno.tab", "sort_122"),
    ("kOrderSid", "bitno.tab", "sort_SID"),
    # LSF split-VQ (modes hors 12.2) + prediction MA
    ("kMeanLsf3",   "q_plsf_3.tab", "mean_lsf"),
    ("kPredFac3",   "q_plsf_3.tab", "pred_fac"),
    ("kDico1Lsf3",  "q_plsf_3.tab", "dico1_lsf"),
    ("kDico2Lsf3",  "q_plsf_3.tab", "dico2_lsf"),
    ("kDico3Lsf3",  "q_plsf_3.tab", "dico3_lsf"),
    ("kMr515Dico3", "q_plsf_3.tab", "mr515_3_lsf"),
    ("kMr795Dico1", "q_plsf_3.tab", "mr795_1_lsf"),
    ("kPastRqInit", "q_plsf_3.tab", "past_rq_init"),
    # LSF split-matrix (12.2)
    ("kMeanLsf5",  "q_plsf_5.tab", "mean_lsf"),
    ("kDico1Lsf5", "q_plsf_5.tab", "dico1_lsf"),
    ("kDico2Lsf5", "q_plsf_5.tab", "dico2_lsf"),
    ("kDico3Lsf5", "q_plsf_5.tab", "dico3_lsf"),
    ("kDico4Lsf5", "q_plsf_5.tab", "dico4_lsf"),
    ("kDico5Lsf5", "q_plsf_5.tab", "dico5_lsf"),
    # gains
    ("kQuaGainPitch",     "gains.tab",    "qua_gain_pitch"),
    ("kQuaGainCode",      "gains.tab",    "qua_gain_code"),
    ("kGainHighRates",    "qua_gain.tab", "table_gain_highrates"),
    ("kGainLowRates",     "qua_gain.tab", "table_gain_lowrates"),
    ("kGainMr475",        "qgain475.tab", "table_gain_MR475"),
    # codebook algebrique : gray + sous-ensembles de pistes
    ("kGray",      "gray.tab",    "gray"),
    ("kDGray",     "gray.tab",    "dgray"),
    ("kStartPos475515", "c2_9pf.tab",  "startPos"),
    ("kStartPos59A",    "c2_11pf.tab", "startPos1"),
    ("kStartPos59B",    "c2_11pf.tab", "startPos2"),
    # etat initial LSP (domaine cosinus Q15)
    ("kLspInit", "lsp.tab", "lsp_init_data"),
    # dispersion de phase (anti-sparseness)
    ("kPhImpLow795", "ph_disp.tab", "ph_imp_low_MR795"),
    ("kPhImpMid795", "ph_disp.tab", "ph_imp_mid_MR795"),
    ("kPhImpLow",    "ph_disp.tab", "ph_imp_low"),
    ("kPhImpMid",    "ph_disp.tab", "ph_imp_mid"),
]

out = []
out.append("// ============================================================================")
out.append("// NkAmrTables.inc — donnees normatives AMR-NB.")
out.append("// Tables extraites MECANIQUEMENT (script scratchpad/amr/extract_26073_tabs.py")
out.append("// + extract_26101_order.py + gen_tables_inc.py) depuis :")
out.append("//   - 3GPP TS 26.073 (livraison ANSI-C officielle du standard, 26073-i00) :")
out.append("//     fichiers de DONNEES *.tab uniquement (initialisateurs de tableaux).")
out.append("//   - 3GPP TS 26.101 v18.0.0 Annexe B (tables B.1-B.8, ordonnancement des")
out.append("//     bits) extraites du .docx officiel ; recoupees avec bitno.tab (26.073).")
out.append("// AUCUNE logique/code n'a ete extraite — donnees numeriques du standard")
out.append("// uniquement, meme pratique que NkAv1Tables.inc (tables de la spec).")
out.append("// Provenance detaillee par table : voir l'en-tete de chaque tableau.")
out.append("// ============================================================================")
out.append("")
for cname, f, sname in WANT:
    data = by_key[(f, sname)]
    out.append("// %s : %s / %s (%d valeurs)" % (cname, f, sname, len(data)))
    out.append("static const nk_int16 %s[%d] = {" % (cname, len(data)))
    for i in range(0, len(data), 16):
        out.append("\t" + ", ".join(str(x) for x in data[i:i + 16]) + ",")
    out[-1] = out[-1].rstrip(",")
    out.append("};")
    out.append("")

open("NkAmrTables.inc", "w").write("\n".join(out) + "\n")
print("NkAmrTables.inc genere : %d tables" % len(WANT))
# controle overflow int16
for cname, f, sname in WANT:
    for v in by_key[(f, sname)]:
        assert -32768 <= v <= 32767, (cname, v)
print("valeurs int16 OK")
