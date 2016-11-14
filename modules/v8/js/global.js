// Global JavaScript functions
function range(from_idx, to, step) {

	switch (arguments.length) {
		case 0:
			throw Error('Too few arguments');
			return;
		case 1:
			var count = +from_idx;
			var arr = []
			if (count < 0) return arr;
			for (var i = 0; i < count; i++) {
				arr[i] = i;
			}
			return arr;
		case 2:
			from_idx = +from_idx;
			to = +to;

			var arr = [];
			if (to < from_idx) return arr;

			for(var i = from_idx; i < to; i++) {
				arr[i - from_idx] = i;
			}
			return arr;
		case 3:
			from_idx = +from_idx;
			to = +to;
			step = +step;

			if (step == 0) throw Error('Step argument is zero!');

			var arr = []
			if (from_idx >= to && step > 0) return arr;
			if (from_idx <= to && step < 0) return arr;

			var count = 0;
			if (step > 0) {
				count = ((to - from_idx - 1) / step) + 1;
			} else {
				count = ((from_idx - to - 1) / -step) + 1;
			}

			var idx = 0;
			if (step > 0) {
				for (var i = from_idx; i < to; i += step) {
					arr[idx++] = i;
				}
			} else {
				for (var i = from_idx; i > to; i += step) {
					arr[idx++] = i;
				}
			}
			return arr;
		default:
			throw Error('Too much arguments');
	}
}
