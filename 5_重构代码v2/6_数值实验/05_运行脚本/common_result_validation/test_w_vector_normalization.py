from normalize_w_vector import normalize,metrics

def test_exact(): assert normalize('0.2;0.3;0',5)['status']=='UNRECONSTRUCTABLE'
def test_all_zero(): assert len(normalize('0;0;0;0;0',5)['values'])==5
def test_middle_zero(): assert normalize('0.2;0;0.4;0;0.1',5)['values'][1]==0
def test_metrics(): assert abs(metrics([0.2,0,0.4])["mean_all_candidate_w"]-(0.6/3))<1e-12
