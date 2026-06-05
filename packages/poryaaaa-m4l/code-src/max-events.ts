export type MaxAtom = string | number;

export type MaxEventHandler<Args extends readonly MaxAtom[] = MaxAtom[]> = (
    ...args: Args
) => void;

export type MaxEventHandlers<Names extends string> = Record<
    Names,
    MaxEventHandler
>;
