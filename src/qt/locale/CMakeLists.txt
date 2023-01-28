set(TS_FILES
    bitcoin_af_ZA.ts
    bitcoin_ar.ts
    bitcoin_be_BY.ts
    bitcoin_bg.ts
    bitcoin_ca.ts
    bitcoin_ca@valencia.ts
    bitcoin_ca_ES.ts
    bitcoin_cs.ts
    bitcoin_cy.ts
    bitcoin_da.ts
    bitcoin_de.ts
    bitcoin_el_GR.ts
    bitcoin_en.ts
    bitcoin_eo.ts
    bitcoin_es.ts
    bitcoin_es_CL.ts
    bitcoin_es_DO.ts
    bitcoin_es_MX.ts
    bitcoin_es_UY.ts
    bitcoin_et.ts
    bitcoin_eu_ES.ts
    bitcoin_fa.ts
    bitcoin_fa_IR.ts
    bitcoin_fi.ts
    bitcoin_fr.ts
    bitcoin_fr_CA.ts
    bitcoin_gl.ts
    bitcoin_he.ts
    bitcoin_hi_IN.ts
    bitcoin_hr.ts
    bitcoin_hu.ts
    bitcoin_id_ID.ts
    bitcoin_it.ts
    bitcoin_ja.ts
    bitcoin_ka.ts
    bitcoin_kk_KZ.ts
    bitcoin_ko_KR.ts
    bitcoin_ky.ts
    bitcoin_la.ts
    bitcoin_lt.ts
    bitcoin_lv_LV.ts
    bitcoin_ms_MY.ts
    bitcoin_nb.ts
    bitcoin_nl.ts
    bitcoin_pam.ts
    bitcoin_pl.ts
    bitcoin_pt_BR.ts
    bitcoin_pt_PT.ts
    bitcoin_ro_RO.ts
    bitcoin_ru.ts
    bitcoin_sk.ts
    bitcoin_sl_SI.ts
    bitcoin_sq.ts
    bitcoin_sr.ts
    bitcoin_sv.ts
    bitcoin_th_TH.ts
    bitcoin_tr.ts
    bitcoin_uk.ts
    bitcoin_ur_PK.ts
    bitcoin_vi.ts
    bitcoin_zh_CN.ts
    bitcoin_zh_TW.ts
)

set_source_files_properties(${TS_FILES} PROPERTIES
    OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(LUPDATE)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    qt_add_translation(QM_FILES ${TS_FILES})
endif()

add_custom_target(gridcoinqt_l10n DEPENDS ${QM_FILES})
